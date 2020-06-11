#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#define PAGE_SIZE 4096
#define MAP_SIZE 40960
#define BUF_SIZE 512

#define master_IOCTL_MMAP 0x12345678

size_t get_filesize(const char *filename); //get the size of the input file

int main(int argc, char *argv[])
{
	char buf[512], number_of_file[50];
	int i, dev_fd, file_fd, num_of_file = 0; // the fd for the device and the fd for the input file
	size_t ret, file_size, tmp;
	size_t offset = 0;

	void *mappedMemory, *kernelMemory;

	char *kernel_address = NULL, *file_address = NULL;
	struct timeval start;
	struct timeval end;
	double transmissionTime; //calulate the time between the device is opened and it is closed

	strcpy(number_of_file, argv[1]); // get the N (but in chars)

	for (int i = 0; i < strlen(argv[1]); i++) // convert from chars to int
	{
		num_of_file *= 10;
		num_of_file += argv[1][i] - '0';
	}
	char file_name[num_of_file + 3][50];
	for (int i = 0; i < num_of_file; i++)
	{
		strcpy(file_name[i], argv[i + 2]); // get the N file names
	}

	for (int i = 0; i < num_of_file; i++)
	{

		if ((dev_fd = open("/dev/master_device", O_RDWR)) < 0)
		{
			perror("failed to open /dev/master_device\n");
			return 1;
		}
		gettimeofday(&start, NULL);
		if ((file_fd = open(file_name[i], O_RDWR)) < 0)
		{
			perror("failed to open input file\n");
			return 1;
		}

		if ((file_size = get_filesize(file_name[i])) < 0)
		{
			perror("failed to get filesize\n");
			return 1;
		}

		if (ioctl(dev_fd, 0x12345677) == -1) //0x12345677 : create socket and accept the connection from the slave
		{
			perror("ioclt server create socket error\n");
			return 1;
		}

		switch (argv[num_of_file + 2][0])
		{
		case 'f': //fcntl : read()/write()
			do
			{
				ret = read(file_fd, buf, sizeof(buf)); // read from the input file
				write(dev_fd, buf, ret);			   //write to the the device
			} while (ret > 0);
			break;
		default:
		;
			int reg = i * MAP_SIZE;
			kernelMemory = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, 0);
			for (i = 0; reg < file_size; i++)
			{

				tmp = file_size - reg;
				if (tmp > MAP_SIZE)
				{
					tmp = MAP_SIZE;
				}

				mappedMemory = mmap(NULL, tmp, PROT_READ, MAP_SHARED, file_fd, reg);
				memcpy(kernelMemory, mappedMemory, tmp);
				munmap(mappedMemory, tmp);
				while (errno == EAGAIN && ioctl(dev_fd, master_IOCTL_MMAP, tmp) < 0)
					;
			}
			if (ioctl(dev_fd, 0x111, kernelMemory) == -1)
			{
				fprintf(stderr, "ioclt server error\n");
				return 1;
			}
			munmap(kernelMemory, MAP_SIZE);
			break;
		}

		if (ioctl(dev_fd, 0x12345679) == -1) // end sending data, close the connection
		{
			perror("ioclt server exits error\n");
			return 1;
		}
		gettimeofday(&end, NULL);
		transmissionTime = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) * 0.0001;
		printf("Transmission time: %lf ms, File size: %lu bytes\n", transmissionTime, file_size);

		close(file_fd);
		close(dev_fd);
	}
	return 0;
}

size_t get_filesize(const char *filename)
{
	struct stat st;
	stat(filename, &st);
	return st.st_size;
}

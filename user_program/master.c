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

#define master_IOCTL_CREATESOCK 0x12345677
#define master_IOCTL_MMAP 0x12345678
#define master_IOCTL_EXIT 0x12345679

size_t get_filesize(const char *filename); //get the size of the input file

int openmaster_device(int *dev_fd)
{
	if ((*dev_fd = open("/dev/master_device", O_RDWR)) < 0)
	{
		perror("failed to open /dev/master_device\n");
		return 1;
	}
}

int checkioctl(int *dev_fd)
{
	if (ioctl(*dev_fd, 0x12345677) == -1) //0x12345677 : create socket and accept the connection from the slave
	{
		perror("ioctl server create socket error\n");
		return 1;
	}
}

int main(int argc, char *argv[])
{
	char buf[512], number_of_file[50];
	int i, dev_fd, file_fd, num_of_file = 0; // the fd for the device and the fd for the input file
	size_t ret, file_size, tmp;
	size_t total_file_size = 0;

	void *mappedMemory, *kernelMemory;

	char *kernel_address = NULL, *file_address = NULL;
	struct timeval start;
	struct timeval end;

	double transmissionTime; //calulate the time between the device is opened and it is closed
	double total_transmissionTime = 0;

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
		size_t offset = 0;

		openmaster_device(&dev_fd);

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

		checkioctl(&dev_fd);

		gettimeofday(&start, NULL);

		switch (argv[num_of_file + 2][0])
		{
		case 'f': //fcntl : read()/write()
			do
			{
				ret = read(file_fd, buf, sizeof(buf)); // read from the input file
				write(dev_fd, buf, ret);			   //write to the the device
			} while (ret > 0);
			break;

		case 'm':
			kernelMemory = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, 0);
			for (int j = 0; j * MAP_SIZE < file_size; j++)
			{
				tmp = file_size - j * MAP_SIZE;
				if (tmp > MAP_SIZE)
					tmp = MAP_SIZE;
				mappedMemory = mmap(NULL, tmp, PROT_READ, MAP_SHARED, file_fd, j * MAP_SIZE);
				memcpy(kernelMemory, mappedMemory, tmp);
				munmap(mappedMemory, tmp);
				while (ioctl(dev_fd, master_IOCTL_MMAP, tmp) < 0 && errno == EAGAIN)
					;
			}
			if (ioctl(dev_fd, 0x111, kernelMemory) == -1)
			{
				perror("ioclt server error\n");
				return 1;
			}
			munmap(kernelMemory, MAP_SIZE);
			break;
		}

		while (ioctl(dev_fd, master_IOCTL_EXIT) < 0 && errno == EAGAIN)
			; // end sending data, close the connection
		gettimeofday(&end, NULL);
		transmissionTime = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) * 0.0001;
		//printf("Transmission time: %lf ms, File size: %lu bytes\n", transmissionTime, file_size);
		total_transmissionTime += transmissionTime;
		total_file_size += file_size;

		close(file_fd);
		close(dev_fd);
	}
	printf("Transmission time: %lf ms, File size: %lu bytes\n", total_transmissionTime, total_file_size);
	return 0;
}

size_t get_filesize(const char *filename)
{
	struct stat st;
	stat(filename, &st);
	return st.st_size;
}

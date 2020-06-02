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

#define master_IOCTL_CREATESOCK 0x12345677
#define master_IOCTL_MMAP 0x12345678
#define master_IOCTL_EXIT 0x12345679

#define PAGE_SIZE 4096
#define MAP_SIZE 40960
size_t GET_filesize(const char *filename); //get the size of the input file

int main(int argc, char *argv[]) //argv[1]: file argv[num_of_file+2]: method
{
	char buf[512], number_of_file[50];
	int i, dev_fd, file_fd, num_of_file; // the fd for the device and the fd for the input file
	size_t file_size, offset = 0, tmp;
	ssize_t tmpp;

	struct timeval begin, finish;
	double transmissionTime; //calulate the time between the device is opened and it is closed
	void *mappedMemory, *kernelMemory;

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
	{ // process one by one (N times)

		if ((dev_fd = open("/dev/master_device", O_RDWR)) < 0)
		{
			fprintf(stderr, "failed to open /dev/master_device\n");
			return 1;
		}
		gettimeofday(&begin, NULL);
		if ((file_fd = open(file_name[i], O_RDWR)) < 0)
		{
			fprintf(stderr, "failed to open input file\n");
			return 1;
		}

		if ((file_size = GET_filesize(file_name[i])) < 0)
		{
			fprintf(stderr, "failed to get filesize\n");
			return 1;
		}

		if (ioctl(dev_fd, master_IOCTL_CREATESOCK) == -1) //master_IOCTL_CREATESOCK : create socket and accept the connection from the slave
		{
			fprintf(stderr, "ioclt server create socket error\n");
			return 1;
		}

		if (argv[num_of_file + 2][0] == 'f')
		{
			do
			{
				tmpp = read(file_fd, buf, sizeof(buf)); // read from the input file
				while (errno == EAGAIN && write(dev_fd, buf, tmpp) < 0)
					; //write to the the device
			} while (tmpp > 0);
		}
		else
		{
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
		}

		while (errno == EAGAIN && ioctl(dev_fd, master_IOCTL_EXIT) < 0)
			; // end sending data, close the connection
		gettimeofday(&finish, NULL);
		transmissionTime = (finish.tv_sec - begin.tv_sec) * 1000 + (finish.tv_usec - begin.tv_usec) * 0.0001;
		printf("Master: Transmission time: %lf ms, File size: %ld bytes\n", transmissionTime, file_size);
		close(dev_fd);
		close(file_fd);
	}
	return 0;
}

size_t GET_filesize(const char *filename)
{
	struct stat st;
	stat(filename, &st);
	return st.st_size;
}

//referrence: https://github.com/qazwsxedcrfvtg14/OS-Proj2

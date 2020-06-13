#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#define slave_IOCTL_CREATESOCK 0x12345677
#define slave_IOCTL_MMAP 0x12345678
#define slave_IOCTL_EXIT 0x12345679

#define PAGESZ 4096
#define MAPSZ 40960
char buf[512], method[50], ip[50], number_of_file[50];
int i, dev_fd, fd, num_of_file = 0; // the fd for the device and the fd for the input file
double transmissionTime;			//calulate the time between the device is opened and it is closed
void *mapped_mem, *kernel_mem;
size_t fileSize = 0;
ssize_t val;
struct timeval start;
struct timeval end;

void ioctlerror()
{
	fprintf(stderr, "ioctl error\n");
	return;
}

int checkerror1()
{
	if ((dev_fd = open("/dev/slave_device", O_RDWR)) < 0)
	{ //should be O_RDWR for PROT_WRITE when mmap()
		fprintf(stderr, "failed to open /dev/slave_device\n");
		return 1;
	}
	return 0;
}

int checkerror2()
{
	if (ioctl(dev_fd, 0x111, kernel_mem) == -1)
	{
		ioctlerror();
		return 1;
	}
	return 0;
}

int checkerror3()
{
	if (ioctl(dev_fd, slave_IOCTL_CREATESOCK, ip) == -1)
	{ //slave_IOCTL_CREATESOCK : connect to master in the device
		fprintf(stderr, "ioctl create slave socket error\n");
		return 1;
	}
	return 0;
}

int checkerror4()
{
	if (ioctl(dev_fd, slave_IOCTL_EXIT) == -1)
	{ // end receiving data, close the connection
		fprintf(stderr, "ioctl client exits error\n");
		return 1;
	}
	return 0;
}
void writesuccess()
{
	write(1, "ioctl success\n", 14);
	return;
}

void callfcntl()
{
	do
	{
		while ((val = read(dev_fd, buf, sizeof(buf))) < 0 && errno == EAGAIN)
			;				 // read from the the device
		write(fd, buf, val); //write to the input file
		fileSize += val;
	} while (val > 0);
	return;
}

int main(int argc, char *argv[])
{

	strcpy(number_of_file, argv[1]); // get the N (but in chars)

	for (int i = 0; i < strlen(argv[1]); i++)
	{ // convert from chars to int
		num_of_file *= 10;
		num_of_file += argv[1][i] - '0';
	}

	char filename[num_of_file + 3][50];
	for (int i = 0; i < num_of_file; i++)
	{
		strcpy(filename[i], argv[i + 2]); // get the N file names
	}

	strcpy(method, argv[num_of_file + 2]);
	strcpy(ip, argv[num_of_file + 3]);

	for (int i = 0; i < num_of_file; i++)
	{
		fileSize = 0;
		int ok = checkerror1();
		if (ok == 1)
			return 1;

		if ((fd = open(filename[i], O_RDWR | O_CREAT | O_TRUNC)) < 0)
		{
			fprintf(stderr, "failed to open input file\n");
			return 1;
		}

		int ok3 = checkerror3();
		if (ok3 == 1)
			return 1;

		gettimeofday(&start, NULL);

		writesuccess();
		if (strcmp(method, "fcntl") == 0)
			callfcntl();
		else if (strcmp(method, "mmap") == 0)
		{ // if method is mmap
			kernel_mem = mmap(NULL, MAPSZ, PROT_READ, MAP_SHARED, dev_fd, 0);
			while (1)
			{
				while ((val = ioctl(dev_fd, slave_IOCTL_MMAP)) < 0 && errno == EAGAIN)
					;

				if (val < 0)
				{
					ioctlerror();
					return 1;
				}
				else if (val == 0)
					break;

				posix_fallocate(fd, fileSize, val);
				size_t offset = (fileSize / PAGESZ);
				offset *= PAGESZ;
				size_t offlen = fileSize;
				offlen -= offset;

				mapped_mem = mmap(NULL, offlen + val, PROT_WRITE, MAP_SHARED, fd, offset);
				memcpy(mapped_mem + offlen, kernel_mem, val);
				munmap(mapped_mem, offlen + val);
				fileSize += val;
			}
			ftruncate(fd, fileSize);

			int ok2 = checkerror2();
			if (ok2 == 1)
				return 1;
			munmap(kernel_mem, MAPSZ);
			//break;
		}

		int ok4 = checkerror4();
		if (ok4 == 1)
			return 1;

		gettimeofday(&end, NULL);
		transmissionTime = (end.tv_usec - start.tv_usec) * 0.0001 + (end.tv_sec - start.tv_sec) * 1000;
		printf("Slave: Transmission time: %lf ms, File size: %ld bytes\n", transmissionTime, fileSize);

		close(fd);
		close(dev_fd);
	}
	return 0;
}

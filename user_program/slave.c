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
#define PageSize 4096
#define MapSize 40960

int main(int argc, char* argv[]){
	char buf[512], number_of_file[50], method[50], ip[50]; //512 = BUF Size
	size_t SizeOfFile = 0;	ssize_t tmp;
	double transmissionTime;
	void *mappedMem, *kernelMem;
	int dev_fd, file_fd, num_of_file = 0;
	struct timeval start_time, end_time;
	/*for (int i=1;i<argc;i++){
		printf("%d\n",(int)strlen(argv[i]));
	}*/
		
	strcpy(number_of_file,argv[1]); // get the N (but in chars)

	for (int i=0;i<strlen(argv[1]);i++){ // convert from chars to int
		num_of_file *= 10;
		num_of_file += argv[1][i]-'0';
	}
	//printf("%d\n",num_of_file );
	
	char file_name[num_of_file+3][50];
	for (int i=0; i<num_of_file; i++){
		strcpy (file_name[i], argv[i+2]); // get the N file names
	}
	/*for (int i=0; i<num_of_file; i++){
		printf("%s\n", file_name[i]);
	}*/


	strcpy (method, argv[num_of_file+2]);
	strcpy (ip, argv[num_of_file+3]);
	// printf("%s\n", method);
	// printf("%s\n", ip);
	
	for (int i=0; i<num_of_file; i++){ // process one by one (N times)

		if( (dev_fd = open("/dev/slave_device", O_RDWR)) < 0){ 
			fprintf(stderr, "failed to open /dev/slave_device\n");
			return 1;
		}

		gettimeofday(&start_time ,NULL);
		if( (file_fd = open (file_name[i], O_RDWR | O_TRUNC | O_CREAT)) < 0){
			fprintf(stderr, "failed to open input file\n");
			return 1;
		}

		if(ioctl(dev_fd, slave_IOCTL_CREATESOCK, ip) == -1){	
			fprintf(stderr, "ioctl create slave socket error\n");
			return 1;
		}

		write (1, "ioctl success\n", 14);

		if (strcmp(method, "fcntl")==0){ // if method is fcntl (read/write)
			do{
				while(errno==EAGAIN && (tmp = read(dev_fd, buf, sizeof(buf)))<0); // get input from the device (read)
				write(file_fd, buf, tmp); // write
				SizeOfFile += tmp;
			} while(tmp > 0);
		}

		else if (strcmp(method, "mmap")==0){ // if method is mmap
			kernelMem = mmap(NULL, MapSize, PROT_READ, MAP_SHARED, dev_fd, 0);
			while(1){
				while( errno==EAGAIN && (tmp = ioctl(dev_fd, slave_IOCTL_MMAP))<0);
				
				if(tmp < 0){
					fprintf(stderr, "ioctl error\n");
					return 1;
				}
				
				if(tmp == 0) break;

				posix_fallocate(file_fd, SizeOfFile, tmp);
				
				size_t offset = (SizeOfFile / PageSize);  offset *= PageSize;
				size_t offlen = SizeOfFile; offlen-=offset;
				
				mappedMem = mmap(NULL, offlen+tmp, PROT_WRITE, MAP_SHARED, file_fd, offset);
				memcpy(mappedMem+offlen, kernelMem, tmp);
				munmap(mappedMem, offlen+tmp);
				SizeOfFile += tmp;
			}

			ftruncate(file_fd, SizeOfFile); //truncate  file_fd to a specified length 
			if(ioctl(dev_fd, 0x111, kernelMem) == -1){
				fprintf(stderr, "ioctl error\n");
				return 1;
			}
			
			munmap(kernelMem, MapSize);
		}

		if(ioctl(dev_fd, slave_IOCTL_EXIT) == -1){
			fprintf(stderr, "ioctl client exits error\n");
			return 1;
		}

		gettimeofday(&end_time, NULL);
		transmissionTime = (double)(end_time.tv_usec - start_time.tv_usec) /(double)10000 + (double)(end_time.tv_sec - start_time.tv_sec)*(double)1000; // calculate the transmission time
		printf("Slave: Transmission time: %lf ms, File size: %ld bytes\n", transmissionTime, SizeOfFile );


		close(file_fd);
		close(dev_fd);

	}
	return 0;
}

//referrence: https://github.com/qazwsxedcrfvtg14/OS-Proj2

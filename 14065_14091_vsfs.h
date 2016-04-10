#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
/* 
Min number of blocks - 64 [262144 B]
Max number of blocks - 2056 [8421376 B]

1 - Super Block
2 - Inode Bitmap
3 - Data Bitmap
4..8 - Inodes [256*80]
9..N - User_Data 
*/

typedef struct _inode {
	int inode_number;
	long file_size;
	long block_size;
	int block_num;
	int direct_links[15];
}inode;							//Total size 42 B, on disk 256 B

typedef struct _pair {
	int inode_number;
	char file_name[32];
}pair;							//34 B

typedef struct _dir {
	int inode_number;
	int file_num;
	int block_num;
	int map_location;
}dir;							//8 B

typedef struct _super_block {
	char filesystem[256];
	int num_inodes;
	int num_data_blocks;
	int inode_map_start;
}super_block;					//Total size 262 B, on Disk 4096 B

int inode_bitmap[80];	//Total size 4096 B
int *data_bitmap;	//Total size 4096 B (min - 56 && max - 2048)
dir *root;

char *user_data;
char *fileSystemName;

int fileSystemDesc;
int data_blocks;
int size_file;

//helper functions
void write_inode (inode *in) {
	int offset = (4096*3) + (256*in->inode_number);
	FILE * file= fopen(fileSystemName, "rb+");
	fseek(file,offset,SEEK_SET);
	fwrite(in,sizeof(inode),1,file);
	fclose(file);
	return;
}

int generate_inode_number () {
	int flag =0;
	for (int i=0;i<80;i++){
		if (inode_bitmap[i]==0) {
			flag = i;
			break;
		}
	}
	inode_bitmap[flag] = 1;
	return flag;
}

int get_data_bitmap () {
	int flag =0;
	for (int i=0;i<data_blocks;i++){
		if (data_bitmap[i]==0) {
			flag = i;
			break;
		}
	}
	data_bitmap[flag] = 1;
	return flag;
}

void update_disk () {
	FILE * file= fopen(fileSystemName, "rb+");
	fwrite(inode_bitmap,sizeof(int),80,file);
	fseek(file,4096*2,SEEK_SET);

	fwrite(data_bitmap,sizeof(int),data_blocks,file);
	fseek(file,4096*3,SEEK_SET);

	fwrite(root, sizeof(dir), 1, file);
	fclose(file);
	return;
}

void write_to_directory (int inode_number, char* filename) {
	pair *mapping = malloc(sizeof(pair));
	int offset = 28672 + (52*(inode_number-1));

	mapping->inode_number = inode_number;
	strcpy(mapping->file_name,filename); 
	FILE * file= fopen(fileSystemName, "rb+");
	fseek(file,offset,SEEK_SET);
	fwrite(mapping,sizeof(pair),1,file);
	fclose(file);
	return;
}

//APIs to implement FileSystem APIs
int createSFS(char* filename, int nbytes){
	super_block *super;

	data_blocks = (nbytes-32768)/4096;
	if (nbytes%4096!=0 || nbytes<262144 || nbytes>8421376) {
		fprintf(stderr,"File System size not acceptable, needs to be in the range 262144 Bytes to 8421376 Bytes and has to be a multiple of 4096 Bytes\n");
		exit(1);
	}
	else  {
		super = malloc (sizeof(super_block));
		strcpy(super->filesystem,"vsfs");
		super->num_inodes = 0;
		super->num_data_blocks = 0;
		super->inode_map_start = 12288;

		root = malloc(sizeof(dir));
		root->inode_number = 0;
		root->file_num = 0;
		root->block_num = 0;
		root->map_location = 28672;

		//inode_bitmap = malloc (sizeof(int) * 80);
		//memset(inode_bitmap,0,sizeof(int) * 80);
		for (int i=0;i<80;i++) {
			inode_bitmap[i] = 0;
		}
		inode_bitmap[0] = 1;

		data_bitmap = malloc(sizeof(int) * data_blocks);
		for (int i=0;i<data_blocks;i++) {
			data_bitmap[i] = 0;
		}
		data_bitmap[0] = 1;

		int filedesc = open(filename,O_RDWR|O_APPEND|O_CREAT,S_IRUSR|S_IWUSR|S_IXUSR);
		close(filedesc);

		FILE * file= fopen(filename, "wb");
		fwrite(super, sizeof(super_block), 1, file);
		fseek(file,4096,SEEK_SET);

		fwrite(inode_bitmap,sizeof(int),80,file);
		fseek(file,4096*2,SEEK_SET);

		fwrite(data_bitmap,sizeof(int),data_blocks,file);
		fseek(file,4096*3,SEEK_SET);

		fwrite(root, sizeof(dir), 1, file);
		fclose(file);
		return filedesc;
	}
	return -1;
}
int readData(int disk, int blockNum, void* block) {
	if (blockNum == 0) {
		fprintf(stderr,"Restricted Data Block\n");
		exit(0);
	}
	else {
		char *data_block = malloc(4096);
		int offset = 32768 + (4096*blockNum);
		FILE * file= fopen(fileSystemName, "rb");
		fseek(file,offset,SEEK_SET);
		fread(data_block,sizeof(char),4096,file);
		fclose(file);
		printf("%s\n",data_block);
		return strlen(data_block);
	}
}
int writeData(int disk, int blockNum, void* block){
	if (blockNum == 0) {
		fprintf(stderr,"Restricted Data Block\n");
		exit(0);
	}
	else {
		char *data_block = (char *)block;
		int offset = 32768 + (4096*blockNum);
		FILE * file= fopen(fileSystemName, "rb+");
		fseek(file,offset,SEEK_SET);
		fwrite(data_block,sizeof(char),strlen(data_block),file);
		fclose(file);
		return strlen(data_block);
	}
}

//FileSystem APIs
int writeFile(int disk, char* filename, void* block) {
	int j,k;
	inode *in;
	char *data = malloc(sizeof(char) * 4096);
	char *data_block = (char *)block;
	in = malloc(sizeof(inode));
	in->inode_number = generate_inode_number();
	in->file_size = strlen(data_block);
	in->block_size = 4096;
	in->block_num = (int) ((strlen(data_block)/4096)+1);
	for (int i=0;i<in->block_num;i++) {
		for (j=4096*i,k=0;j<(4096*i)+4096;j++,k++) {
			data[k] = data_block[j]; 
		}
		in->direct_links[i] = get_data_bitmap();
		int temp_val = writeData(fileSystemDesc,in->direct_links[i],data_block);
	}
	for (int i=in->block_num;i<15;i++) {
		in->direct_links[i] = 0;
	}

	write_inode(in);
	root->file_num += 1;
	root->block_num += in->block_num;
	update_disk();
	write_to_directory(in->inode_number,filename);
	// for (int i=0;i<strlen(data_block);i++) {
	// 	printf("%c\n",data_block[i]);
	// }
	return in->inode_number;

}
int readFile(int disk, char* filename, void* block){
	int count = 0;
	int offset;
	int in_num,bytes;
	pair *mapping = malloc(sizeof(pair));
	inode *in = malloc(sizeof(inode));

	for (int i=0;i<80;i++) {
		if (inode_bitmap[i]==1) {
			count++;
		}
		else {
			break;
		}
	}
	FILE * file= fopen(fileSystemName, "rb");
	for (int i=0;i<count-1;i++) {
		offset = 28672 + (52*(i));
		fseek(file,offset,SEEK_SET);
		fread(mapping, sizeof(pair), 1, file);
		if (strcmp(mapping->file_name,filename)==0) {
			in_num = mapping->inode_number;
			break;
		}
	}
	offset = (4096*3) + (256*in_num);
	fseek(file,offset,SEEK_SET);
	fread(in, sizeof(inode),1,file);
	size_file = in->file_size;
	for (int i=0;i<15;i++) {
		if (in->direct_links[i] != 0) {
			bytes = readData(fileSystemDesc,in->direct_links[i],(char *)block);
		}
		else {
			break;
		}
	}
	fclose(file);
	return bytes;
}

//Diagnostic APIs
void print_inodeBitmaps(int fileSystemId) {
	for (int i=0;i<80;i++) {
		printf("%d ",inode_bitmap[i]);
	}
	printf("\n");
}
void print_dataBitmaps(int fileSystemId) {
	for (int i=0;i<data_blocks;i++) {
		printf("%d ",data_bitmap[i]);
	}
	printf("\n");
}
void print_FileList(int fileSystemId) {
	int count = 0;
	int offset;
	pair *mapping = malloc(sizeof(pair));
	for (int i=0;i<80;i++) {
		if (inode_bitmap[i]==1) {
			count++;
		}
		else {
			break;
		}
	}
	FILE * file= fopen(fileSystemName, "rb");
	for (int i=0;i<count-1;i++) {
		offset = 28672 + (52*(i));
		fseek(file,offset,SEEK_SET);
		fread(mapping, sizeof(pair), 1, file);
		printf("%d %s\n",mapping->inode_number,mapping->file_name);
	}
	fclose(file);
	return;
}


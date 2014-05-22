/*
 *	linux/fs/hw1-rsuvvari/hw1/sys_xconcat.c
 *
 *	2014, Raghavendra Suvvari
 */

#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define SUCCESS 0
#define PAGE_SIZE_COPY 4096

asmlinkage extern long (*sysptr)(void *arg);
static int concatFiles(void *args, int argslen);
static int validateInputs(void *args, int argslen);

struct userArgs {
	__user const char *outfile;
	__user const char **infiles;
	unsigned int infile_count;
	int oflags;
	mode_t mode;
	unsigned int flags;
};

struct userArgs usrargs;

/****************************************************************
Function name	:	xconcat
Inputs		:	args, argslen
Output		:	Written bytes or Percentage or Number of
				files succesfully read, if success
			-1,	if failure
*****************************************************************/
asmlinkage long xconcat(void *args, int argslen)
{
	ssize_t retval = SUCCESS;

	/*Checking whether the inputs given by the user are valid or not*/
	retval = validateInputs(args, argslen);
	if (retval < SUCCESS)
		return retval;

	/*If the validation is success then call the function
				to concat for the given modes*/
	else
		retval = concatFiles(args, argslen);

	return retval;
}

/****************************************************************
Function name   :       validateInputs
Inputs          :       args, argslen
Output          :       0,	if success
			-1,     if failure
*****************************************************************/

static int validateInputs(void *args, int argslen)
{
	struct userArgs *usrArgs ;
	ssize_t ret = SUCCESS;
	struct file *wfilp = NULL ;
	int i = 0;
	struct file *fd = NULL;

	usrArgs = kmalloc(sizeof(struct userArgs), GFP_KERNEL);

	if (usrArgs == NULL) {
		printk(KERN_DEBUG "ERROR: error occured in memory allocation\n");
		ret = -ENOMEM;
		goto cleanup;
	}
	/* Copy the structure from user space to address space */
	ret = copy_from_user(usrArgs, args, argslen);
	if (ret < 0) {
		printk(KERN_DEBUG "ERROR: error occured from copy_from_user\n");
		ret = -EFAULT;
		goto cleanup;
	}

	if (argslen != sizeof(struct userArgs)) {
		printk(KERN_DEBUG "ERROR : error because of argslen is 0\n");
		ret = -EINVAL;
		goto cleanup;
	}

	/* Check whether the given input file exists or not*/
	if (usrArgs->outfile == NULL) {
		printk(KERN_DEBUG "ERROR : Outfile doesn't exist\n");
		ret = -EFAULT;
		goto cleanup;
	}

	/* Check whether the number of files is 0 or more than 10*/
	if (usrArgs->infile_count == 0) {
		printk(KERN_DEBUG "ERROR : No input files\n");
		ret = -E2BIG;
		goto cleanup;
	}

	if (usrArgs->infile_count > 10) {
		printk(KERN_DEBUG "ERROR : Too many input files, maximum allowed is 10\n");
		ret = -EFAULT;
		goto cleanup;
	}

	/* Combination of N, P flags is not allowed*/
	if (usrArgs->flags == 0x03 || usrArgs->flags == 0x07) {
		printk(KERN_DEBUG "ERROR : Bad combination of flags, NP combination of flags is not allowed\n");
		ret = -EFAULT;
		goto cleanup;
	}

	usrArgs->outfile = getname((char *)usrArgs->outfile);

	/* If the given flags contain TRUNC then we have to pass the user flags
		 by removing inorder not to corrupt the existing output file */
	if (usrArgs->oflags & O_TRUNC)
		wfilp = filp_open(usrArgs->outfile,
				usrArgs->oflags ^ O_TRUNC, usrArgs->mode);
	else
		wfilp = filp_open(usrArgs->outfile, usrArgs->oflags,
						usrArgs->mode);

	if (!(wfilp) || IS_ERR(wfilp)) {
		printk(KERN_DEBUG "ERROR : output file is corrupted\n");
		ret = PTR_ERR(wfilp);
		goto cleanup;
	}

	if (!wfilp->f_op->write) {
		printk(KERN_DEBUG "ERROR : Not able to perform write operation in output file\n");
		ret = -EFAULT;
		goto cleanup;
	}

	/* Open the input files one by one and check
		whether any of them are same as the given output file*/
	for (i = 0; i < usrArgs->infile_count; i++) {
		fd = filp_open(usrArgs->infiles[i], O_RDONLY, 0);
		if (!fd || IS_ERR(fd)) {
			printk(KERN_DEBUG "ERROR : Error occurred while openeing input file\n");
			ret = -EFAULT;
			goto closeFile;
		}
		if (fd->f_dentry->d_inode == wfilp->f_dentry->d_inode) {
			printk(KERN_DEBUG "ERROR : Outfile is same as one of the input files\n");
			ret = -EINVAL;
			goto closeFile;
		}
closeFile:
		if ((fd) && !(IS_ERR(fd)))
			filp_close(fd, NULL);
	}

	/* Do the clean up of all the opened files and allocated memory*/
cleanup:

	putname(usrArgs->outfile);

	kfree(usrArgs);

	if ((wfilp) && !(IS_ERR(wfilp)))
		filp_close(wfilp, NULL);

	return ret;
}
/****************************************************************
Function name   :       concatFiles
Inputs          :       args, argslen
Output          :	Written bytes or Percentage or Number of
			files succesfully read, if success
			-1,     if failure
*****************************************************************/
static int concatFiles(void *args, int argslen)
{
	int i = 0;
	int j = 0;
	ssize_t ret = 0;
	struct userArgs *usrArgs;
	struct file *wfilp = NULL;
	int bytes = 0;
	int wbytes = 0;
	int prcnt = 0;
	int readBytes = 0;
	int writeBytes = 0;
	int filesRead = 0;
	int totalSize = 0;

	mm_segment_t old_fs;
	void *buf = NULL;
	struct file *fd = NULL;
	struct kstat stat;
	int atomic = 0;

	/* The variables used in the atomic mode are declared in this section */
#ifdef EXTRA_CREDIT
	struct file *temp = NULL;
	mode_t tempMode;
	char *tmp = "tempfile.tmp";
	struct dentry *tempDentry = NULL;
	struct dentry *writeDentry = NULL;
	struct inode *tempDir = NULL;
	struct inode *writeDir = NULL;
	struct file *wfilp1 = NULL;
	int tempReadBytes = 0;
	int tempWbytes = 0;

#endif

	usrArgs = kmalloc(sizeof(struct userArgs), GFP_KERNEL);

	if (usrArgs == NULL) {
		printk(KERN_DEBUG "ERROR : Memory couldn't be allocated\n");
		ret =  -ENOMEM;
		goto cleanup;
	}

	ret = copy_from_user(usrArgs, args, argslen);

	if (ret < 0) {
		printk(KERN_DEBUG "ERROR : Error occurred while copying from user space to kernel space\n");
		ret = -EFAULT;
		goto cleanup;
	}
	if (usrArgs->flags & 0x04)
		atomic = 1;
	else
		atomic = 0;

	usrArgs->outfile = getname((char *)usrArgs->outfile);

	/* A buffer of size 4096 bytes is taken to copy*/
	buf = kmalloc(PAGE_SIZE_COPY, GFP_KERNEL);
	if (buf == NULL) {
		printk(KERN_DEBUG "ERROR : Memory couldn't be allocated\n");
		ret = -ENOMEM;
		goto cleanup;
	}

if (atomic) {
#ifdef EXTRA_CREDIT

	/* If the 'O_CREAT' and 'O_EXCL' combination is given then we have
		already	created the file in validation function while
		checking whether the output file and input files are same
		or not. So we need to remove the 'O_EXCL' from the given flags*/

	if ((usrArgs->oflags & O_CREAT) == O_CREAT &&
				(usrArgs->oflags & O_EXCL) == O_EXCL)
		usrArgs->oflags = usrArgs->oflags ^ O_EXCL;

	/* Inorder not to corrupt the output file when we get O_TRUNC
			we pass O_RDWR to open it*/
	if (usrArgs->oflags & O_TRUNC)
		wfilp1 = filp_open(usrArgs->outfile, O_RDWR, usrArgs->mode);
	else
		wfilp1 = filp_open(usrArgs->outfile,
				usrArgs->oflags, usrArgs->mode);

	if (!(wfilp1) || IS_ERR(wfilp1)) {
		printk(KERN_DEBUG "ERROR : Error occured while opening the temp file\n");
		ret = PTR_ERR(wfilp1);
		goto cleanup;
	}

	if (!wfilp1->f_op->write) {
		printk(KERN_DEBUG "ERROR : error occured in write operation second time\n");
		ret = -EFAULT;
		if ((usrArgs->oflags & O_CREAT) == O_CREAT)
			vfs_unlink(writeDir, writeDentry);
		goto cleanup;
	}

	tempMode = wfilp1->f_dentry->d_inode->i_mode;
	temp = filp_open(tmp, O_RDWR | O_CREAT, tempMode);

	if (!(temp) || IS_ERR(temp)) {
		printk(KERN_DEBUG "ERROR : Error occured while opening the temp file\n");
		ret = PTR_ERR(temp);
		goto cleanup;
	}

	if (!temp->f_op->write) {
		printk(KERN_DEBUG "ERROR : error occured in write operation second time\n");
		ret = -EFAULT;
		if ((usrArgs->oflags & O_CREAT) == O_CREAT)
			vfs_unlink(writeDir, writeDentry);
		goto cleanup;
	}

	/* To find the total size of all the input flags we use vfs_stat*/
	for (j = 0; j < usrArgs->infile_count; j++) {
		vfs_stat(usrArgs->infiles[j], &stat);
		totalSize = totalSize + stat.size;
	}

	temp->f_pos = 0;
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* Copy the data from given output file to a temp file*/
	if (usrArgs->oflags & O_APPEND) {
		do {
			tempReadBytes = vfs_read(wfilp1, buf, PAGE_SIZE_COPY,
								&wfilp1->f_pos);
			tempWbytes = vfs_write(temp, buf, tempReadBytes,
								&temp->f_pos);
		} while (tempReadBytes >= PAGE_SIZE_COPY);
	}
	set_fs(old_fs);

	tempDir = temp->f_dentry->d_parent->d_inode;
	tempDentry = temp->f_dentry;
	writeDir = wfilp1->f_dentry->d_parent->d_inode;
	writeDentry = wfilp1->f_dentry;

	/*copy all the data from input files to a output file */
	for (j = 0; j < usrArgs->infile_count; j++) {
		fd = filp_open(usrArgs->infiles[j], O_RDONLY, 0);
		if (!fd->f_op->read) {
			printk(KERN_DEBUG "ERROR : Error occured while opening read file\n");
			ret = -EINVAL;
			goto cleanup;
		}
		fd->f_pos = 0;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		do {
			bytes = vfs_read(fd, buf, PAGE_SIZE_COPY, &fd->f_pos);
			/* if the read or write process failed in middle then
				close the temp file and delink it and do
					the appropriate clean up*/
			if (bytes < 0) {
				filp_close(temp, NULL);
				vfs_unlink(tempDir, tempDentry);
				ret = bytes;
				goto closeFile1;
			}
			wbytes = vfs_write(temp, buf, bytes, &temp->f_pos);
			if (wbytes < 0) {
				filp_close(temp, NULL);
				vfs_unlink(tempDir, tempDentry);
				ret = wbytes;
				goto closeFile1;
			} else
				writeBytes = writeBytes + wbytes;
		} while (bytes >= PAGE_SIZE_COPY);
	filesRead = filesRead + 1;
	set_fs(old_fs);
	}

	vfs_rename(tempDir, tempDentry, writeDir, writeDentry);
	prcnt = ((writeBytes/totalSize)*100);

closeFile1:
		filp_close(fd, NULL);
		if ((usrArgs->oflags & O_CREAT) == O_CREAT)
			vfs_unlink(writeDir, writeDentry);
#else
	ret = -EINVAL;
	printk(KERN_DEBUG "ERROR: Atomic mode is turned off\n");
	goto cleanup;
#endif
}
	/* The below code is for the non-atomic mode */
else {
	/* If the 'O_CREAT' and 'O_EXCL' combination is given then we have
		already	created the file in validation function while checking
		whether the output file and input files are same or not.
		So we need to remove the 'O_EXCL' from the given flags*/

	if ((usrArgs->oflags & O_CREAT) == O_CREAT &&
			(usrArgs->oflags & O_EXCL) == O_EXCL)
		wfilp = filp_open(usrArgs->outfile,
				usrArgs->oflags ^ O_EXCL, usrArgs->mode);
	else
		wfilp = filp_open(usrArgs->outfile,
					usrArgs->oflags, usrArgs->mode);

	if (!(wfilp) || IS_ERR(wfilp)) {
		printk(KERN_DEBUG "ERROR : Error occured while opening the ouput file\n");
		ret = PTR_ERR(wfilp);
		goto cleanup;
	}

	if (!wfilp->f_op->write) {
		printk(KERN_DEBUG "ERROR : error occured in write operation second time\n");
		ret = -EFAULT;
		goto cleanup;
	}

	for (j = 0; j < usrArgs->infile_count; j++) {
		vfs_stat(usrArgs->infiles[i], &stat);
		totalSize = totalSize + stat.size;
	}

	wfilp->f_pos = 0;
	for (j = 0; j < usrArgs->infile_count; j++) {
		fd = filp_open(usrArgs->infiles[j], O_RDONLY, 0);
		if (!fd->f_op->read) {
			printk(KERN_DEBUG "ERROR : Error occured while opening read file\n");
			ret = -EINVAL;
			goto cleanup;
		}
		fd->f_pos = 0;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		do {
			bytes = vfs_read(fd, buf, PAGE_SIZE_COPY, &fd->f_pos);
			if (bytes < 0)
				goto closeFile;
			else
				readBytes = readBytes + bytes;

			wbytes = vfs_write(wfilp, buf, bytes, &wfilp->f_pos);
			if (wbytes < 0)
				goto closeFile;
			else
				writeBytes = writeBytes + wbytes;
		} while (bytes >= PAGE_SIZE_COPY);

		filesRead = filesRead + 1;
		set_fs(old_fs);
closeFile:
		if ((fd) && !(IS_ERR(fd)))
			filp_close(fd, NULL);
	}

	prcnt = ((writeBytes/totalSize)*100);
}
	/* Close all the files and deallocate the memory at the end*/
cleanup:

	putname(usrArgs->outfile);
	if ((wfilp) && !(IS_ERR(wfilp)))
		filp_close(wfilp, NULL);

	if ((fd) && !(IS_ERR(fd)))
		filp_close(fd, NULL);

	if (buf != NULL)
		kfree(buf);

	kfree(usrArgs);

	if (ret < 0)
		return ret;
	else {
	switch (usrArgs->flags) {
	case 0x01:
	case 0x01 | 0x04:
				return filesRead;
				break;
	case 0x02:
	case 0x02 | 0x04:
				return prcnt;
				break;
	default:
				return writeBytes;
				break;
		}
	}
}

static int __init init_sys_xconcat(void)
{
	printk(KERN_INFO "installed new sys_xconcat module\n");
	if (sysptr == NULL)
		sysptr = xconcat;
	return SUCCESS;
}
static void  __exit exit_sys_xconcat(void)
{
	if (sysptr != NULL)
		sysptr = NULL;
	printk(KERN_INFO "removed sys_xconcat module\n");
}
module_init(init_sys_xconcat);
module_exit(exit_sys_xconcat);
MODULE_LICENSE("GPL");

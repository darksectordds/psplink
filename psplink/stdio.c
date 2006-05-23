/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * stdio.c - PSPLINK kernel module tty code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 *
 * $HeadURL$
 * $Id$
 */

/* Based off the pspsdk tty code */

#include <pspkernel.h>
#include <pspdebug.h>

static int g_initialised = 0;
static PspDebugInputHandler g_stdin_handler = NULL;
static PspDebugPrintHandler g_stdout_handler = NULL;
static PspDebugPrintHandler g_stderr_handler = NULL;
static SceUID g_in_sema = 0;
/* Probably stdout and stderr should not be guarded by the same mutex */
static SceUID g_out_sema = 0;

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

static int io_init(PspIoDrvArg *arg)
{
	return 0;
}

static int io_exit(PspIoDrvArg *arg)
{
	return 0;
}

static int io_open(PspIoDrvFileArg *arg, char *file, int mode, SceMode mask)
{
	if((arg->fs_num != STDIN_FILENO) && (arg->fs_num != STDOUT_FILENO) && (arg->fs_num != STDERR_FILENO))
	{
		return SCE_KERNEL_ERROR_NOFILE;
	}

	return 0;
}

static int io_close(PspIoDrvFileArg *arg)
{
	return 0;
}

static int io_read(PspIoDrvFileArg *arg, char *data, int len)
{
	int ret = 0;

	(void) sceKernelWaitSema(g_in_sema, 1, 0);
	if((arg->fs_num == STDIN_FILENO) && (g_stdin_handler != NULL))
	{
		ret = g_stdin_handler(data, len);
	}
	(void) sceKernelSignalSema(g_in_sema, 1);

	return ret;
}

static int io_write(PspIoDrvFileArg *arg, const char *data, int len)
{
	int ret = 0;

	(void) sceKernelWaitSema(g_out_sema, 1, 0);
	if((arg->fs_num == STDOUT_FILENO) && (g_stdout_handler != NULL))
	{
		ret = g_stdout_handler(data, len);
	}
	else if((arg->fs_num == STDERR_FILENO) && (g_stderr_handler != NULL))
	{
		ret = g_stderr_handler(data, len);
	}
	(void) sceKernelSignalSema(g_out_sema, 1);

	return ret;
}

static int io_lseek(PspIoDrvFileArg *arg, u32 unk, long long ofs, int whence)
{
	return 0;
}

static int io_ioctl(PspIoDrvFileArg *arg, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
	return 0;
}

static int io_remove(PspIoDrvFileArg *arg, const char *name)
{
	return 0;
}

static int io_mkdir(PspIoDrvFileArg *arg, const char *name, SceMode mode)
{
	return 0;
}

static int io_rmdir(PspIoDrvFileArg *arg, const char *name)
{
	return 0;
}

static int io_dopen(PspIoDrvFileArg *arg, const char *dir)
{
	return 0;
}

static int io_dclose(PspIoDrvFileArg *arg)
{
	return 0;
}

static int io_dread(PspIoDrvFileArg *arg, SceIoDirent *dir)
{
	return 0;
}

static int io_getstat(PspIoDrvFileArg *arg, const char *file, SceIoStat *stat)
{
	return 0;
}

static int io_chstat(PspIoDrvFileArg *arg, const char *file, SceIoStat *stat, int bits)
{
	return 0;
}

static int io_rename(PspIoDrvFileArg *arg, const char *oldname, const char *newname)
{
	return 0;
}

static int io_chdir(PspIoDrvFileArg *arg, const char *dir)
{
	return 0;
}

static int io_mount(PspIoDrvFileArg *arg)
{
	return 0;
}

static int io_umount(PspIoDrvFileArg *arg)
{
	return 0;
}

static int io_devctl(PspIoDrvFileArg *arg, const char *devname, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
	return 0;
}

static int io_unknown(PspIoDrvFileArg *arg)
{
	return 0;
}

static PspIoDrvFuncs tty_funcs = 
{
	io_init,
	io_exit,
	io_open,
	io_close,
	io_read,
	io_write,
	io_lseek,
	io_ioctl,
	io_remove,
	io_mkdir,
	io_rmdir,
	io_dopen,
	io_dclose,
	io_dread,
	io_getstat,
	io_chstat,
	io_rename,
	io_chdir,
	io_mount,
	io_umount,
	io_devctl,
	io_unknown,
};

static PspIoDrv tty_driver = 
{
	"tty", 0x10, 0x800, "TTY", &tty_funcs
};

static int tty_init(void)
{
	int ret;
	(void) sceIoDelDrv("tty"); /* Ignore error */
	ret = sceIoAddDrv(&tty_driver);
	if(ret < 0)
	{
		return ret;
	}

	g_in_sema = sceKernelCreateSema("TtyInMutex", 0, 1, 1, NULL);
	if(g_in_sema < 0)
	{
		return g_in_sema;
	}

	g_out_sema = sceKernelCreateSema("TtyOutMutex", 0, 1, 1, NULL);
	if(g_out_sema < 0)
	{
		return g_out_sema;
	}

	ret = sceIoReopen("tty0:", PSP_O_RDONLY, 0777, sceKernelStdin());
	if(ret < 0)
	{
		return ret;
	}

	ret = sceKernelStdoutReopen("tty1:", PSP_O_WRONLY, 0777);
	if(ret < 0)
	{
		return ret;
	}

	ret = sceKernelStderrReopen("tty2:", PSP_O_WRONLY, 0777);
	if(ret < 0)
	{
		return ret;
	}

	g_initialised = 1;

	return 0;
}

int stdioInstallStdinHandler(PspDebugInputHandler handler)
{
	if(g_initialised == 0)
	{
		int ret;
		ret = tty_init();
		if(ret < 0)
		{
			return ret;
		}
	}

	g_stdin_handler = handler;

	return 0;
}

int stdioInstallStdoutHandler(PspDebugPrintHandler handler)
{
	if(g_initialised == 0)
	{
		int ret;
		ret = tty_init();
		if(ret < 0)
		{
			return ret;
		}
	}

	g_stdout_handler = handler;

	return 0;
}

int stdioInstallStderrHandler(PspDebugPrintHandler handler)
{
	if(g_initialised == 0)
	{
		int ret;
		ret = tty_init();
		if(ret < 0)
		{
			return ret;
		}
	}

	g_stderr_handler = handler;

	return 0;
}

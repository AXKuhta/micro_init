// Build with:
// aarch64-linux-gnu-gcc-10 -nostdlib -static -fno-asynchronous-unwind-tables -fno-ident -s -Os -o micro_init micro_init.c -lgcc

// COMPILER FLAGS EXPLANATION:
// [REQUIRED] `-nostdlib` tells the compiler to forget about using glibc
// [REQUIRED] `-static` removes dependency on `/lib64/ld-linux-x86-64.so.2`
// [OPTIONAL] `-s` will remove debugging information
// [OPTIONAL] `-Os` instructs to optimize for code size
// [OPTIONAL] `-fno-ident` will remove the `GCC: (Ubuntu 10.2.0-5ubuntu1~20.04) 10.2.0` string from the file
// [OPTIONAL] `-fno-asynchronous-unwind-tables` will remove `.eh_frame` section from the file, saving 2 KB
// [OPTIONAL] `-lgcc` (Must come last) will allow to use __builtin_strlen(), which nolibc can benefit from

// You can take nolibc.h from here:
// https://github.com/torvalds/linux/blob/master/tools/include/nolibc/nolibc.h
#include "nolibc.h"

// Just substitute it quickly
void printf(char* string) {
	write(1, string, strlen(string));
}

#define COLOR_YELLOW "\x1b[33m"
#define COLOR_RESET "\x1b[0m"

// Print a warning but don't stop
void warn(char* message) {
	printf(COLOR_YELLOW "[WARNING] ");
	printf(message);
	printf(COLOR_RESET);
}

// Print a error and stop
void err(char* message) {
	printf(COLOR_YELLOW "[ERROR] ");
	printf(message);
	printf("Stopping...");

	while (1) {
	}
}

// Hostile environment, no easy way to build strings
// Just keep a bunch of prebuilt ones
char* lut[] = { "/dev/loop0",
		"/dev/loop1",
		"/dev/loop2",
		"/dev/loop3",
		"/dev/loop4",
		"/dev/loop5",
		"/dev/loop6",
		"/dev/loop7",
		"/dev/loop8",
		"/dev/loop9",
		"/dev/loop10",
		"/dev/loop11",
		"/dev/loop12",
		"/dev/loop13",
		"/dev/loop14",
		"/dev/loop15" };

// include/uapi/linux/loop.h
#define LOOP_CTL_GET_FREE 0x4C82
#define LOOP_SET_FD 0x4C00
#define LOOP_CLR_FD 0x4C01

// include/uapi/linux/mount.h
#define MS_RDONLY 1
#define MS_BIND 4096

#define IMAGE_PATH "/ext2.img"
#define IMAGE_FS_TYPE "ext2"
#define TARGET_DIRECTORY "/newroot"

// Create a loop device and mount the rootfs image
void mount_ext2_image() {
	int ctl_fd = open("/dev/loop-control", O_RDWR, 0);

	if (ctl_fd < 0)
		err(	"Failed to open /dev/loop-control;\n"
			"1. Is folder `dev` missing on real root and devtmpfs didn't initialize?\n"
			"2. Is loop device support compiled in?\n"					);

	int loop_num = ioctl(ctl_fd, LOOP_CTL_GET_FREE, NULL);
	if (loop_num < 0) err(	"Request to spawn a new loop device was denied\n"
				"Is folder `dev` missing on real root and devtmpfs didn't initialize?\n" );

	char* loop_path = lut[loop_num];
	int loop_fd = open(loop_path, O_RDWR, 0);
	if (loop_fd < 0) err("Failed to open the dispensed loop???\n");

	int image_fd = open(IMAGE_PATH, O_RDWR, 0);
	if (image_fd < 0) err("Failed to open [" IMAGE_PATH "]!\n" );

	ioctl(loop_fd, LOOP_SET_FD, image_fd);

	int rc = mount(loop_path, TARGET_DIRECTORY, IMAGE_FS_TYPE, MS_RDONLY, NULL);

	if (rc) err("Failed to mount the loop into [" TARGET_DIRECTORY "]!\n");
}

// Mount mmcblk0p1 -> /boot
void mount_boot() {
	int rc = mount("/dev/mmcblk0p1", "/boot", "vfat", MS_RDONLY, NULL);

	if (rc) warn("Failed to mount [/dev/mmcblk0p1] into [/boot]!\n"
				"Device numeration may be off...\n");
}

// Bind folder A to folder B
int mount_bind(char* source, char* destination) {
	return mount(source, destination, NULL, MS_BIND, NULL);
}

// Will make the following binds:
// /dev -> /newroot/dev
// See Void Linux init scripts to get an insight into starting a Linux system:
// https://github.com/void-linux/void-runit/tree/master/core-services
void bind_dev() {
	int rc = 0;

	rc = mount_bind("/dev", TARGET_DIRECTORY "/dev");
	if (rc) err(	"Error binding [/dev] to [" TARGET_DIRECTORY "/dev]\n"
			"Perhaps /dev is missing in the rootfs you're using?\n" );
}

// Will make fresh mounts of:
// /dev/shm 			Ramdisk
// /dev/pts 			Pseudoterminals
void mount_shm_pts() {
	int rc = 0;

	rc = mkdir("/dev/shm", 0777);
	if (rc) err("Failed to create [/dev/shm]\n");

	rc = mount("shm", "/dev/shm", "tmpfs", 0, NULL);
	if (rc) err("Error mounting [/dev/shm]\n");

	rc = mkdir("/dev/pts", 0755);
	if (rc) err("Failed to create [/dev/pts]\n");

	rc = mount("devpts", "/dev/pts", "devpts", 0, NULL);
	if (rc) err("Error mounting [/dev/pts]\n");
}

// Will make a fresh mount of /proc
// Task managers (And a ton of other stuff) will not work without this mounted
void mount_procfs() {
	int rc = mount("proc", "/proc", "proc", 0, NULL);
	if (rc) err(	"Error mounting procfs into [/proc]\n"
			"Perhaps /proc is missing in the rootfs you're using?\n"  );
}

// Will make a fresh mount of /sys
// `lsblk` and `df` (And a ton of other stuff) will not work without this mounted
void mount_sysfs() {
	int rc = mount("sysfs", "/sys", "sysfs", 0, NULL);
	if (rc) err(    "Error mounting sysfs into [/sys]\n"
			"Perhaps /sys is missing in the rootfs you're using?\n"  );
}

// Executables have a hardcoded list of paths with the .so files they need
// Without chrooting kernel's ELF loader will be unable to find them
void set_root() {
	chroot("/newroot");
	chdir("/");
}

// Ubuntu Base does not have any init system as it turns out
// Just launch bash then
// (Using `su -`)
//
// "can't access tty; job control turned off" workarounds:
// https://www.linux.org.ru/forum/general/6211553
// Adding --pty fixed it
void exec_shell() {
	char* argv[] = { "su", "-", "--pty", NULL };
	char* envp[] = { "HOME=/", "TERM=linux", NULL };

	printf(COLOR_YELLOW "Dropping you into a root shell so you can set a password or create a new account. If done, use Ctrl + Alt + F2 to F12 to switch into a real console.\n" COLOR_RESET);

	int rc = execve("/bin/su", argv, envp);

	if (rc)
		err("exec_shell: failed to start shell\n");
}

void exec_agetty(char* tty) {
	pid_t agetty_pid = fork();

	if (agetty_pid < 0)
		err("exec_agetty: fork error\n");

	// Always restart agetty
	if (agetty_pid > 0) {
		waitpid(agetty_pid, NULL, 0);
		exec_agetty(tty);
	};

	setsid();

	char* argv[] = { "agetty", "", NULL };
	char* envp[] = { "HOME=/", "TERM=linux", NULL };

	argv[1] = tty;

	int rc = execve("/sbin/agetty", argv, envp);

	if (rc)
		err("exec_agetty: execve error\n");
}

void start_every_tty() {
	char* ttys[] = { "tty2", "tty3", "tty4", "tty5", "tty6", "tty7", "tty8", "tty9", "tty10", "tty11", "tty12", NULL };
	int i = 0;

	while (ttys[i]) {
		pid_t pid = fork();

		if (pid < 0)
			err("start_every_tty: fork error\n");

		if (pid == 0)
			exec_agetty(ttys[i]);

		i++;
	}
}

#define MNT_DETACH 2

// For clean shutdowns
void unmount_root() {
	int rc = umount2("/", MNT_DETACH);

	if (rc)
		err("unmount_root: failed to unmount\n");
}

int main() {
	printf("= = = Micro Init = = =\n");

	//mount_ext2_image();
	//bind_dev();
	//set_root();

	// Fork into two separate processes
	// Parent will receive shell_pid = child pid
	// Child will receive shell_pid = 0
	pid_t shell_pid = fork();

	// If we are the child, start the shell
	if (shell_pid == 0) {
		// Critical mounts
		mount_shm_pts();
		mount_procfs();
		mount_sysfs();

		// Optional mounts
		mount_boot();

		start_every_tty();
		exec_shell();

		// Function above should never return
		// But let's just make sure that we can't continue from this location
		return 0;
	} else {
		waitpid(shell_pid, NULL, 0);

		printf("Initial shell exited, entering shutdown sequence\n");
		printf("Unmounting root...\n");

		unmount_root();

		printf(COLOR_YELLOW "It is now safe to turn off your computer\n" COLOR_RESET);

		while (1) { 
			// Wait indefinitely
		}
	}


	return 0;
}


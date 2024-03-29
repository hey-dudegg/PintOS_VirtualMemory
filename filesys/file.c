#include "filesys/file.h"
#include <debug.h>
#include "filesys/inode.h"
#include "threads/malloc.h"

/* An open file. */
/*
inode는 파일 시스템에서 파일을 나타내는 핵심 데이터 구조입니다.
각 파일은 고유한 inode를 가지며, 이 inode에는 파일의 메타 데이터(예: 파일 유형, 크기, 소유자 및 권한 등)가 저장됩니다.
또한 inode에는 파일의 데이터 블록에 대한 포인터가 있어서 파일의 실제 내용을 저장할 수 있습니다.
파일 시스템은 inode를 사용하여 파일의 위치와 관련된 모든 정보를 추적합니다.
파일을 열 때 파일 시스템은 해당 파일의 inode를 찾고 해당 inode를 사용하여 파일을 읽거나 쓸 수 있습니다.
*/
struct file {
	struct inode *inode;        /* File's inode. 파일의 메타 데이터 */
	off_t pos;                  /* Current position. 파일 내 현재 위치 */
	bool deny_write;            /* Has file_deny_write() been called? */
};

/* Opens a file for the given INODE, of which it takes ownership,
 * and returns the new file.  Returns a null pointer if an
 * allocation fails or if INODE is null. */
struct file *
file_open (struct inode *inode) {
	struct file *file = calloc (1, sizeof *file);
	if (inode != NULL && file != NULL) {
		file->inode = inode;
		file->pos = 0;
		file->deny_write = false;
		return file;
	} else {
		inode_close (inode);
		free (file);
		return NULL;
	}
}

/* Opens and returns a new file for the same inode as FILE.
 * Returns a null pointer if unsuccessful. */
struct file *
file_reopen (struct file *file) {
	return file_open (inode_reopen (file->inode));
}

/* Duplicate the file object including attributes and returns a new file for the
 * same inode as FILE. Returns a null pointer if unsuccessful. */
struct file *
file_duplicate (struct file *file) {
	struct file *nfile = file_open (inode_reopen (file->inode));
	if (nfile) {
		nfile->pos = file->pos;
		if (file->deny_write)
			file_deny_write (nfile);
	}
	return nfile;
}

/* Closes FILE. */
void
file_close (struct file *file) {
	if (file != NULL) {
		file_allow_write (file);
		inode_close (file->inode);
		free (file);
	}
}

/* Returns the inode encapsulated by FILE. */
struct inode *
file_get_inode (struct file *file) {
	return file->inode;
}

/* Reads SIZE bytes from FILE into BUFFER,
 * starting at the file's current position.
 * Returns the number of bytes actually read,
 * which may be less than SIZE if end of file is reached.
 * Advances FILE's position by the number of bytes read. */


/* FILE에서 현재 위치부터 SIZE 바이트를 BUFFER로 읽습니다.
실제로 읽은 바이트 수를 반환하며, 파일의 끝에 도달하면 SIZE보다 작을 수 있습니다.
읽은 바이트 수만큼 FILE의 위치를 이동합니다. */
off_t
file_read (struct file *file, void *buffer, off_t size) {
	off_t bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
	file->pos += bytes_read;
	return bytes_read;
}

/* Reads SIZE bytes from FILE into BUFFER,
 * starting at offset FILE_OFS in the file.
 * Returns the number of bytes actually read,
 * which may be less than SIZE if end of file is reached.
 * The file's current position is unaffected. */
off_t
file_read_at (struct file *file, void *buffer, off_t size, off_t file_ofs) {
	return inode_read_at (file->inode, buffer, size, file_ofs);
}

/* Writes SIZE bytes from BUFFER into FILE,
 * starting at the file's current position.
 * Returns the number of bytes actually written,
 * which may be less than SIZE if end of file is reached.
 * (Normally we'd grow the file in that case, but file growth is
 * not yet implemented.)
 * Advances FILE's position by the number of bytes read. */
off_t
file_write (struct file *file, const void *buffer, off_t size) {
	off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
	file->pos += bytes_written;
	return bytes_written;
}

/* Writes SIZE bytes from BUFFER into FILE,
 * starting at offset FILE_OFS in the file.
 * Returns the number of bytes actually written,
 * which may be less than SIZE if end of file is reached.
 * (Normally we'd grow the file in that case, but file growth is
 * not yet implemented.)
 * The file's current position is unaffected. */
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
		off_t file_ofs) {
	return inode_write_at (file->inode, buffer, size, file_ofs);
}

/* Prevents write operations on FILE's underlying inode
 * until file_allow_write() is called or FILE is closed. */
/* 
FILE의 기본 inode에 대한 쓰기 작업을 막습니다. 이 함수가 호출되면 해당 파일에 대한 쓰기 작업이 일시적으로 금지되며,
이후에 `file_allow_write()` 함수가 호출되거나 파일이 닫힐 때까지 이러한 작업이 계속 유지됩니다.
이렇게 함으로써, 파일이 읽기 전용으로 설정될 수 있습니다.
*/
void
file_deny_write (struct file *file) {
	ASSERT (file != NULL);
	if (!file->deny_write) {
		file->deny_write = true;
		inode_deny_write (file->inode);
	}
}

/* Re-enables write operations on FILE's underlying inode.
 * (Writes might still be denied by some other file that has the
 * same inode open.) */
void
file_allow_write (struct file *file) {
	ASSERT (file != NULL);
	if (file->deny_write) {
		file->deny_write = false;
		inode_allow_write (file->inode);
	}
}

/* Returns the size of FILE in bytes. */
off_t
file_length (struct file *file) {
	ASSERT (file != NULL);
	return inode_length (file->inode);
}

/* Sets the current position in FILE to NEW_POS bytes from the
 * start of the file. */
/* 
파일에서의 현재 위치를 새로운 위치로 이동시키는 기능을 수행합니다.
파일에서의 위치는 파일의 시작부터의 오프셋을 기준으로 합니다.
여기서 FILE은 파일을 나타내는 파일 디스크립터를 가리키며, NEW_POS는 파일 내에서의 새로운 위치를 나타냅니다.
이 함수를 호출하면 파일의 현재 위치가 NEW_POS로 설정됩니다.
이 함수를 사용하여 파일 내에서 원하는 위치로 이동하여 데이터를 읽거나 쓸 수 있습니다.
파일의 위치를 변경함으로써 파일 내 특정 부분에 접근할 수 있습니다.
*/
void
file_seek (struct file *file, off_t new_pos) {
	ASSERT (file != NULL);
	ASSERT (new_pos >= 0);
	file->pos = new_pos;
}

/* Returns the current position in FILE as a byte offset from the
 * start of the file. */
off_t
file_tell (struct file *file) {
	ASSERT (file != NULL);
	return file->pos;
}

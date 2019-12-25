#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
 
 
static int sys_halt (void);
static int sys_exit (int status);
static int sys_exec (const char *ufile);
static int sys_wait (tid_t);
static int sys_create (const char *ufile, unsigned initial_size);
static int sys_remove (const char *ufile);
static int sys_open (const char *ufile);
static int sys_filesize (int handle);
static int sys_read (int handle, void *udst_, unsigned size);
static int sys_write (int handle, void *usrc_, unsigned size);
static int sys_seek (int handle, unsigned position);
static int sys_tell (int handle);
static int sys_close (int handle);
static int sys_mmap (int handle, void *addr);
static int sys_munmap (int mapping);
static int sys_chdir (const char *udir);
static int sys_mkdir (const char *udir);
static int sys_readdir (int handle, char *name);
static int sys_isdir (int handle);
static int sys_inumber (int handle);
 
static void syscall_handler (struct intr_frame *);
static void get_arguments (void *, const void *, size_t);
 
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
 
/* System call handler. */
static void
syscall_handler (struct intr_frame *f) 
{
  unsigned type;
  int args[3];
  memset (args, 0, sizeof args);

  get_arguments (&type, f->esp, sizeof type);

  switch(type){
    case SYS_HALT:
      f->eax = sys_halt();
      break;

    case SYS_EXIT:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_exit(args[0]);
      break;

    case SYS_EXEC:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_exec (args[0]);
      break;

    case SYS_WAIT:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_wait (args[0]);
      break;

    case SYS_CREATE:
      get_arguments(args, (uint32_t *) f->esp + 1,  2 * sizeof *args);
      f->eax = sys_create (args[0], args[1]);
      break;

    case SYS_REMOVE:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_remove (args[0]);
      break;

    case SYS_OPEN:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_open (args[0]);
      break;

    case SYS_FILESIZE:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_filesize (args[0]);
      break;

    case SYS_READ:
      get_arguments(args, (uint32_t *) f->esp + 1,  3 * sizeof *args);
      f->eax = sys_read (args[0], args[1], args[2]);
      break;

    case SYS_WRITE:
      get_arguments(args, (uint32_t *) f->esp + 1,  3 * sizeof *args);
      f->eax = sys_write (args[0], args[1], args[2]);
      break;

    case SYS_SEEK:
      get_arguments(args, (uint32_t *) f->esp + 1,  2 * sizeof *args);
      f->eax = sys_seek (args[0], args[1]);
      break;

    case SYS_TELL:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_tell (args[0]);
      break;

    case SYS_CLOSE: 
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_close (args[0]);
      break;

    case SYS_MMAP:
      get_arguments(args, (uint32_t *) f->esp + 1,  2 * sizeof *args);
      f->eax = sys_mmap (args[0], args[1]);
      break;

    case SYS_MUNMAP:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_munmap (args[0]);
      break;

    case SYS_CHDIR:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_chdir (args[0]);
      break;

    case SYS_MKDIR:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_mkdir (args[0]);
      break;

    case SYS_READDIR:
      get_arguments(args, (uint32_t *) f->esp + 1,  2 * sizeof *args);
      f->eax = sys_readdir (args[0], args[1]);
      break;

    case SYS_ISDIR:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_isdir (args[0]);
      break;

    case SYS_INUMBER:
      get_arguments(args, (uint32_t *) f->esp + 1,  1 * sizeof *args);
      f->eax = sys_inumber (args[0]);
      break;
    
    default:
      thread_exit();
      break; 
  }
}
 
/* copy the arguments from user memory to kernel memory  */
static void
get_arguments(void *dst_, const void *usrc_, size_t size)
{
  uint8_t *dst = dst_;
  const uint8_t *usrc = usrc_;
  while (size > 0)
    {
      size_t chunk_size = PGSIZE - pg_ofs (usrc);
      if (chunk_size > size)
        chunk_size = size;

      if (!page_lock (usrc, false))
        thread_exit ();
      /* copy chunk_size bytes of data */
      memcpy (dst, usrc, chunk_size);
      page_unlock (usrc);

      dst += chunk_size;
      usrc += chunk_size;
      size -= chunk_size;
    }
}
 
/* copy the arguments from kernel memory to user memory  */
static void
copy_out (void *udst_, const void *src_, size_t size) 
{
  uint8_t *udst = udst_;
  const uint8_t *src = src_;

  while (size > 0) 
    {
      size_t chunk_size = PGSIZE - pg_ofs (udst);
      if (chunk_size > size)
        chunk_size = size;
      
      if (!page_lock (udst, false))
        thread_exit ();
      memcpy (udst, src, chunk_size);
      page_unlock (udst);

      udst += chunk_size;
      src += chunk_size;
      size -= chunk_size;
    }
}
 
/* put user string into kernel memory */
static char *
copy_in_string (const char *us)
{
  char *upage;
  size_t length = 0;
  /* the kernel string */
  char *ks = palloc_get_page (0);
  if (ks == NULL)
    thread_exit ();

  while(true)
  {
    upage = pg_round_down (us);
    if (!page_lock (upage, false)){
      palloc_free_page (ks);
      thread_exit ();
    }
    for (; us < upage + PGSIZE; us++)
    {
      ks[length++] = *us;/* copying */
      if (*us == '\0')/* done */
      {
        page_unlock (upage);
        return ks;
      }
      /* too long causes error */
      else if (length >= PGSIZE)
      {
        page_unlock (upage);
        palloc_free_page (ks);
        thread_exit ();
      }
    }

    page_unlock (upage);
  }
}


/* search the file of id handle  */
static struct opened_file *
search_fd (int handle)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;
  /* look up in the list of opened file */
  for (e = list_begin (&cur->fds); e != list_end (&cur->fds);
       e = list_next (e))
    {
      struct opened_file *fd;
      fd = list_entry (e, struct opened_file, elem);
      if (fd->handle == handle)
        return fd;
    }
  /* no such an opened file */
  thread_exit ();
}



static struct opened_file *
lookup_file_fd (int handle) 
{
  struct opened_file *fd = search_fd (handle);
  if (fd->file == NULL)
    thread_exit ();
  return fd;
}
 

static struct opened_file *
lookup_dir_fd (int handle) 
{
  struct opened_file *fd = search_fd (handle);
  if (fd->dir == NULL)
    thread_exit ();
  return fd;
}

static int
sys_halt (void)
{
  shutdown_power_off ();
  return 0;
}


static int
sys_exit (int exit_status)
{
  thread_current ()->exit_status = exit_status;
  thread_exit ();
}
 

static int
sys_exec (const char *ufile) 
{
  tid_t tid;
  char *kfile = copy_in_string (ufile);
  tid = process_execute (kfile);
  palloc_free_page (kfile);
  return tid;
}
 

static int
sys_wait (tid_t child) 
{
  return process_wait (child);
}
 

static int
sys_create (const char *ufile, unsigned initial_size) 
{
  char *kfile = copy_in_string (ufile);
  bool ret = filesys_create (kfile, initial_size, FILE_INODE);
  palloc_free_page (kfile);
  return ret;
}
 

static int
sys_remove (const char *ufile) 
{
  char *kfile = copy_in_string (ufile);
  bool ret = filesys_remove (kfile);
  palloc_free_page (kfile);
  return ret;
}


static int
sys_open (const char *ufile) 
{
  char *kfile = copy_in_string (ufile);
  struct opened_file *fd;
  int handle = -1;
 
  fd = calloc (1, sizeof *fd);
  if (fd)
    {
      struct inode *inode = filesys_open (kfile);
      if (inode != NULL)
        {
          if (inode_get_type (inode) == FILE_INODE)
            fd->file = file_open (inode);
          else
            fd->dir = dir_open (inode);
          if (fd->file != NULL || fd->dir != NULL)
            {
              struct thread *cur = thread_current ();
              handle = fd->handle = cur->next_handle++;
              /* put the new of into list */
              list_push_front (&cur->fds, &fd->elem);
            }
          else 
            {
              /* no such a file */
              free (fd);
              inode_close (inode);
            }
        }
    }
  
  palloc_free_page (kfile);
  return handle;
}
 


static int
sys_filesize (int handle) 
{
  struct opened_file *fd = lookup_file_fd (handle);
  return file_length (fd->file);
}
 

static int
sys_read (int handle, void *udst_, unsigned size) 
{
  uint8_t *udst = udst_;
  struct opened_file *fd;
  int bytes_read = 0;

  if (handle != STDIN_FILENO)
    fd = lookup_file_fd (handle);

  while (size > 0) 
    {
      size_t page_left = PGSIZE - pg_ofs (udst);
      size_t to_read = size < page_left ? size : page_left;
      off_t ret;

      if (!page_lock (udst, true)) 
        thread_exit ();

      if (handle != STDIN_FILENO) 
        {
          /* read from input */
          ret = file_read (fd->file, udst, to_read);
        }
      else 
        {
           /* read from input */
          for (size_t i = 0; i < to_read; i++) 
            udst[i] = input_getc ();
          bytes_read = to_read;
        }

      page_unlock (udst);
      /* done, no more spare space for us or error */
      if (ret != to_read)
        return (bytes_read + ret);
      bytes_read += ret;
      udst += ret;
      size -= ret;
    }
   
  return bytes_read;
}
 

static int
sys_write (int handle, void *usrc_, unsigned size) 
{
  uint8_t *usrc = usrc_;
  struct opened_file *fd = NULL;
  int bytes_written = 0;

  if (handle != STDOUT_FILENO)
    fd = lookup_file_fd (handle);

  while (size > 0) 
    {
      size_t page_left = PGSIZE - pg_ofs (usrc);
      size_t to_write = size < page_left ? size : page_left;
      off_t ret;

      if (!page_lock (usrc, false)) 
        thread_exit ();

      if (handle == STDOUT_FILENO)
        {
          /* write to sdt out */
          putbuf ((char *) usrc, to_write);
          ret = to_write;
        }
      else
        /* read from file */
        ret = file_write (fd->file, usrc, to_write);

      page_unlock (usrc);

      /* done, no more spare space for us or error */
      if (ret != to_write)
        return (bytes_written + ret);
      bytes_written += ret;
      usrc += ret;
      size -= ret;
    }
 
  return bytes_written;
}
 

static int
sys_seek (int handle, unsigned position) 
{
  if ((off_t) position >= 0)
    file_seek (lookup_file_fd (handle)->file, position);
  return 0;
}
 

static int
sys_tell (int handle) 
{
  return file_tell (lookup_file_fd (handle)->file);
}
 

static int
sys_close (int handle) 
{
  struct opened_file *fd = search_fd (handle);

  file_close (fd->file);
  dir_close (fd->dir);
  list_remove (&fd->elem);
  free (fd);
  return 0;
}




/* search for the mapping, exit if fail to find it */
static struct mapping *
search_mapping (int handle)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;
   
  for (e = list_begin (&cur->mappings); e != list_end (&cur->mappings);
       e = list_next (e))
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      if (m->handle == handle)
        return m;
    }
 
  thread_exit ();
}

/* remove mapping from the virtual address space,
   write back dirty pages */
void
unmap (struct mapping *m) 
{
  list_remove (&m->elem);
  for(size_t i = 0; i < m->page_cnt; i++)
    page_deallocate((void *) ((m->base) + (PGSIZE * i)));
  file_close (m->file);
  free (m);
}
 

static int
sys_mmap (int handle, void *addr)
{
  struct opened_file *fd = lookup_file_fd (handle);
  struct mapping *m = malloc (sizeof *m);
  size_t offset;
  off_t length;

  if (m == NULL || addr == NULL || pg_ofs (addr) != 0)
    return -1;

  /* some initialization work of mapping */
  m->handle = thread_current ()->next_handle++;
  m->file = file_reopen (fd->file);
  if (m->file == NULL) 
    {
      free (m);
      return -1;
    }
  m->base = addr;
  m->page_cnt = 0;
  /* put it into the list */
  list_push_front (&thread_current ()->mappings, &m->elem);

  offset = 0;
  length = file_length (m->file);
  while (length > 0)
    {
      struct page *p = page_allocate ((uint8_t *) addr + offset, false);
      if (p == NULL)/* allocation failed */
        {
          unmap (m);
          return -1;
        }
      /* some initialization work of page */
      p->private = false;
      p->file = m->file;
      p->file_offset = offset;
      p->file_bytes = length >= PGSIZE ? PGSIZE : length;
      offset += p->file_bytes;
      length -= p->file_bytes;
      m->page_cnt++;
    }
  
  return m->handle;
}

static int
sys_munmap (int mapping)
{
  struct mapping *map = search_mapping(mapping);
  unmap(map);
  return 0;
}


static int
sys_chdir (const char *udir) 
{
  char *kdir = copy_in_string(udir);
  bool ret = filesys_chdir(kdir);
  palloc_free_page(kdir);

  return ret;
}


static int
sys_mkdir (const char *udir)
{
  char *kdir = copy_in_string (udir);
  bool ret = filesys_create (kdir, 0, DIR_INODE);
  palloc_free_page (kdir);
 
  return ret;
}


static int
sys_readdir (int handle, char *uname)
{
  struct opened_file *fd = lookup_dir_fd (handle);
  char name[NAME_MAX + 1];
  if (dir_readdir (fd->dir, name))
  {
    copy_out (uname, name, strlen (name) + 1);
    return true;
  }

  return 0;
  
}


static int
sys_isdir (int handle)
{
  struct opened_file *fd = search_fd (handle);
  bool ret;
  if (fd->dir)
    ret = true;
  else
    ret = false;
  
  return ret;
}


static int
sys_inumber (int handle)
{
  if(sys_isdir(handle))
  {
    struct opened_file *dir_descriptor = lookup_dir_fd(handle);
    struct inode *inode = dir_get_inode(dir_descriptor);
    return inode_get_inumber(inode);
  }

  struct opened_file *fd = search_fd (handle);
  struct inode *inode = file_get_inode (fd->file);
  return inode_get_inumber (inode);
}
 
/* filename.c-- Low level file i/o and routines to deal  */
/*  with filename.                                       */
/* Copyright (C) 1996-1999,2001  Robert Masenten          */
/* This program may be redistributed under the terms of the
   GNU General Public License, version 2; see agility.h for details. */
/*                                                       */
/* This is part of the source for both AGiliTy: the (Mostly) Universal */
/*       AGT Interpreter and for the Magx adventure game compiler.    */

/* Notes for docs:
   file_type_id must be a type that can be set to 0.
 */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "agility.h"

#ifdef force16
#undef int
#endif

#ifdef UNIX_IO
#include <fcntl.h>
#include <sys/stat.h>  /* Needed only for file permission bits */
#include <unistd.h>

#ifdef __STRICT_ANSI__
int fileno(FILE *f);
FILE *popen(char *s,char *how);
int pclose(FILE *p);
#endif

#ifdef MSDOS16
#include <io.h>
#endif
#endif /* UNIX_IO */

#ifdef force16
#define int short
#endif


/*----------------------------------------------------------------------*/
/*  Filetype Data                                                       */
/*----------------------------------------------------------------------*/
const char *extname[]={
  "", 
  DA1, DA2, DA3, DA4, DA5, DA6, DSS,
  pHNT, pOPT, pTTL, 
  pSAV, pSCR, pLOG,
  pAGX, pINS, pVOC, pCFG,
  pAGT, pDAT, pMSG, pCMD, pSTD, AGTpSTD};


#ifdef PATH_SEP
static const char *path_sep=PATH_SEP;
#else
static const char *path_sep=NULL;
#endif

/* This returns the options to use when opening the given file type */
/* rw is true if we are writing, false if we are reading. */
const char *filetype_info(filetype ft, rbool rw)
{
  if (ft<fTTL) return "rb";
  if (ft==fAGX) return rw ? "wb" : "rb";
  if (ft==fSAV) return (rw ? "wb" : "rb");
  if (ft==fTTL || ft==fINS || ft==fVOC) return  "rb";
#ifdef OPEN_AS_TEXT
  if (ft>=fCFG) return (open_as_binary ? "rb" : "r");
#else
  if (ft>=fCFG) return "rb";
#endif
  if (ft==fSCR) {
    if (rw) 
      return (BATCH_MODE || make_test) ? "w" : "a";
    else return "r";
  }
  if (ft==fLOG) return rw ? "w" : "r";
  fatal("INTERNAL ERROR: Invalid filetype.");
  return NULL;
}


/* Returns true if ft is a possible extension in general context ft_base */
static rbool compat_ext(filetype ft, filetype ft_base)
{
  if (ft_base==fNONE || ft_base==fDA1 || ft_base==fAGX) {  /* Game file */
    return ( ft>=fDA1 && ft<=fDSS)
      || ft==fOPT || ft==fTTL 
      || (ft>=fAGX && ft<=fCFG);
  }

  if (ft_base==fSAV || ft_base==fSCR || ft_base==fLOG)
    return (ft==ft_base);

  if (ft_base==fAGT) {  /* Source code */
    return (ft>=fAGT && ft<=fCMD) 
      || ft==fTTL || ft==fCFG;
  }

  fatal("INTERNAL ERROR: Invalid file class.");
  return 0;
}



/*----------------------------------------------------------------------*/
/*  Misc. utilities                                                     */
/*----------------------------------------------------------------------*/

char *assemble_filename(const char *path, const char *root,
			       const char *ext)
{
  int len1, len2, len3;
  char *name;

  len1=len2=len3=0;
  if (path!=NULL) len1=strlen(path); 
  if (root!=NULL) len2=strlen(root); 
  if (ext!=NULL) len3=strlen(ext);
  name=rmalloc(len1+len2+len3+1);
  if (path!=NULL) memcpy(name,path,len1);
#ifdef PREFIX_EXT
  if (ext!=NULL) memcpy(name+len1,ext,len3);
  if (root!=NULL) memcpy(name+len1+len3,root,len2);
#else
  if (root!=NULL) memcpy(name+len1,root,len2);
  if (ext!=NULL) memcpy(name+len1+len2,ext,len3);
#endif
  name[len1+len2+len3]=0;
  return name;
}

/* This works for binary files; we don't care about non-binary
   files since this only used to search for game files. */
static rbool file_exist(const char *fname)
{
  FILE *f;
  f=fopen(fname,"rb");
  if (f==NULL) return 0;
  fclose(f);
  return 1;
}



/* This checks to see if c matches any of the characters in matchset */
static rbool smatch(char c, const char *matchset)
{
  for(;*matchset!=0;matchset++) 
    if (*matchset==c) return 1;
  return 0;
}



/*----------------------------------------------------------------------*/
/*  Taking Apart the Filename                                           */
/*----------------------------------------------------------------------*/

static int find_path_sep(const char *name)
{
  size_t i;

  if (path_sep==NULL) 
    return -1;
  for(i=strlen(name)-1;i>=0;i--)
    if (smatch(name[i],path_sep)) break;
  return i;
}


/* Checks to see if the filename (which must be path-free)
   has an extension. Returns the length of the extensions
   and writes the extension type in pft */
static int search_for_ext(const char *name, filetype base_ft,
			  filetype *pft)
{
  filetype t;
  size_t xlen,len;

  *pft=fNONE;
  len=strlen(name);
  if (len==0) return 0;
  for(t=fNONE+1;t<=fSTD;t++) 
    if (compat_ext(t,base_ft)) {
      xlen=strlen(extname[t]);
      if (xlen==0 || xlen>len) continue;
#ifdef PREFIX_EXT
      if (strncasecmp(name,extname[t],xlen)==0) 
#else
      if (fnamecmp(name+len-xlen,extname[t])==0)
#endif
	{
	  *pft=t;
	  return xlen;
	}
    }
#ifdef UNIX_IO
  /* This is code to make the Unix/etc ports easier to use under
     tab-completing shells (which often complete "gamename._" or
     "gamename.ag_" since there are other files in the directory
     with the same root.) */
  assert(*pft==fNONE);
  if (name[len-1]=='.') return 1;
  if (fnamecmp(name+len-3,".ag")==0) {
    if (base_ft==fDA1 || base_ft==fAGX) *pft=fAGX;
    if (base_ft==fAGT) *pft=fAGT;
  }
  if (fnamecmp(name+len-3,".da")==0) {
    if (base_ft==fDA1 || base_ft==fAGX) *pft=fDA1;
    if (base_ft==fAGT) *pft=fAGT;
  }
  if (*pft!=fNONE) return 3;
#endif
  return 0;
}


/* Extract root filename or extension  from
   pathless name, given that the extension is of length extlen. */
/* If isext is true, extract the extension. If isext is false, 
   then extrac the root. */
static char *extract_piece(const char *name, int extlen, rbool isext)
{
  char *root;
  size_t len, xlen;
  rbool first;  /* If true, extract from beginning; if false, extract
		   from end */

  len=strlen(name)-extlen;
  xlen=extlen;
  if (isext) {
    size_t tmp;
    tmp=len; len=xlen; xlen=tmp;
  }
  if (len==0) return NULL;
  root=rmalloc((len+1)*sizeof(char));
#ifdef PREFIX_EXT
  first=isext ? 1 : 0;  
#else
  first=isext ? 0 : 1;
#endif  
  if (first) {
    memcpy(root,name,len);
    root[len]=0;
  } else {
    memcpy(root,name+xlen,len);
    root[len]=0;
  }
  return root;
}


/* This returns true if "path" is absolute, false otherwise.
   This is _very_ platform dependent. */
static rbool absolute_path(char *path)
{
#ifdef pathtest
  return pathtest(path); 
#else
  return 1;
#endif
}

/*----------------------------------------------------------------------*/
/*  Basic routines for dealing with file contexts                       */
/*----------------------------------------------------------------------*/

#define FC(x) ((file_context_rec*)(x))

/* formal_name is used to get filenames for diagnostic messages, etc. */
char *formal_name(fc_type fc, filetype ft)
{
  if (FC(fc)->special) return FC(fc)->gamename;
  if (ft==fNONE)
    return rstrdup(FC(fc)->shortname);
  if (ft==fAGT_STD)
    return rstrdup(AGTpSTD);
  return assemble_filename("",FC(fc)->shortname,extname[ft]);
}

#ifdef PATH_SEP
static rbool test_file(const char *path, const char *root, const char *ext)
{
  char *name;
  rbool tmp;

  name=assemble_filename(path,root,ext);
  tmp=file_exist(name);
  rfree(name);
  return tmp;
}

/* This does a path search for the game files. */
static void fix_path(file_context_rec *fc)
{
  char **ppath;


  if (gamepath==NULL) return;
  for(ppath=gamepath;*ppath!=NULL;ppath++) 
    if (test_file(*ppath,fc->shortname,fc->ext)
	|| test_file(*ppath,fc->shortname,pAGX)
	|| test_file(*ppath,fc->shortname,DA1))
      {
	fc->path=rstrdup(*ppath);
	return;
      }  
}
#endif


/* This creates a new file context based on gamename. */
/* ft indicates the rough use it will be put towards:
     ft=fNONE indicates it's the first pass read, before PATH has been
         read in, and so the fc shouldn't be filled out until
	 fix_file_context() is called.
     ft=pDA1 indicates that name refers to the game files.
     ft=pAGX indicates the name of the AGX file to be written to.
     ft=pSAV,pLOG,pSCR all indicate that name corresponds to the
         related type of file. */
fc_type init_file_context(const char *name,filetype ft)
{
  file_context_rec *fc;
  size_t p, x;  /* Path and extension markers */

  fc=rmalloc(sizeof(file_context_rec));
  fc->special=0;

#ifdef UNIX
  if (name[0]=='|') {  /* Output pipe */
    name++;
    fc->special=1;
  }
#endif

  fc->gamename=rstrdup(name);  

#ifdef UNIX
  x=strlen(fc->gamename);
  if (fc->gamename[x-1]=='|') {  /* Input pipe */
    fc->gamename[x-1]=0;
    fc->special|=2;
  }
  if (fc->special) {
    fc->path=fc->shortname=fc->ext=NULL;
    return fc;
  }
#endif

  p=find_path_sep(fc->gamename);
  if (p<0) 
    fc->path=NULL;
  else {
    fc->path=rmalloc((p+2)*sizeof(char));    
    memcpy(fc->path,fc->gamename,p+1);
    fc->path[p+1]='\0';
  }
  x=search_for_ext(fc->gamename+p+1,ft,&fc->ft);
  fc->shortname=extract_piece(fc->gamename+p+1,x,0);
  fc->ext=extract_piece(fc->gamename+p+1,x,1);

#ifdef PATH_SEP
  if (fc->path==NULL && ft==fDA1) 
    fix_path(fc);
#endif
  return fc;
}


void fix_file_context(fc_type fc,filetype ft)
{
#ifdef PATH_SEP
  if (FC(fc)->path==NULL && ft==fDA1)   
    fix_path(FC(fc));
#endif
}


/* This creates new file contexts from old. */
/* This is used to create save/log/script filenames from the game name,
   and to create include files in the same directory as the source file. */
fc_type convert_file_context(fc_type fc,filetype ft,const char *name)
{
  file_context_rec *nfc;
  rbool local_ftype; /* Indicates file should be in working directory,
			not game directory. */

  local_ftype=(ft==fSAV || ft==fSCR || ft==fLOG );
  if (BATCH_MODE || make_test) local_ftype=0;

  if (name==NULL) {
    nfc=rmalloc(sizeof(file_context_rec));
    nfc->gamename=NULL;
    nfc->path=NULL;
    nfc->shortname=rstrdup(fc->shortname);
    nfc->ext=NULL;
    nfc->ft=fNONE;
    nfc->special=0;  
  } else {
    nfc=init_file_context(name,ft);
  }   

  /* If path already defined, then combine paths. */
  if (!local_ftype && nfc->path!=NULL && !absolute_path(nfc->path)) {
    char *newpath;
    newpath=nfc->path;
    newpath=assemble_filename(fc->path,nfc->path,"");
    rfree(nfc->path);
    nfc->path=newpath;
  }

  /* scripts, save-games and logs should go in  the working directory, 
     not the game directory, so leave nfc->path equal to NULL for them. */
  if (!local_ftype && nfc->path==NULL)
    nfc->path=rstrdup(fc->path); /* Put files in game directory */
  return nfc;
}

void release_file_context(fc_type *pfc)
{
  file_context_rec *fc;
  fc=FC(*pfc);
  rfree(fc->gamename);
  rfree(fc->path);
  rfree(fc->shortname);
  rfree(fc->ext);
  rfree(fc);
}


/*----------------------------------------------------------------------*/
/*   Routines for Finding Files                                         */
/*----------------------------------------------------------------------*/

#ifdef UNIX
/* This requires that no two sav/scr/log files be open at the same time. */
static int pipecnt=0;
static FILE *pipelist[6];

static genfile try_open_pipe(fc_type fc, filetype ft, rbool rw)
{
  FILE *f;

  errno=0;
  if (ft!=fSAV && ft!=fSCR && ft!=fLOG) return NULL;
  if (rw && fc->special!=1) return NULL; 
  if (!rw && fc->special!=2) return NULL;
  if (pipecnt>=6) return NULL;

  f=popen(fc->gamename,rw ? "w" : "r"); /* Need to indicate this is a pipe */
  pipelist[pipecnt++]=f;
  return f;
}
#endif


static genfile try_open_file(const char *path, const char *root, 
			     const char *ext, const char *how,
			     rbool nofix)
{
  char *name;
  FILE *f;

  name=assemble_filename(path,root,ext);
  f=fopen(name,how);
#ifndef MSDOS
#ifndef GLK
  if (f==NULL && !nofix) { /* Try uppercasing it. */
    char *s;
    for(s=name;*s!=0;s++)
      *s=toupper(*s);
    f=fopen(name,how);
  }
#else
  /*
   * Try converting just the file name extension to uppercase.  This
   * helps us to handle cases where AGT games are unzipped without
   * filename case conversions - in these cases, the files will carry
   * extensions .TTL, .DA1, and so on, whereas we'll be looking for
   * files with extensions .ttl, .da1, etc.  This is one part of
   * being file name case insensitive - the other part is to define
   * fnamecmp as strcasecmp, which we do in config.h.
   */
  if (f == NULL && !nofix && ext != NULL) {
    char *uc_ext;
    int  i;

    /* Free the existing name. */
    rfree (name);

    /* Convert the extension to uppercase. */
    uc_ext = rmalloc (strlen (ext) + 1);
    for (i = 0; i < strlen (ext); i++)
      uc_ext[i] = toupper (ext[i]);
    uc_ext[strlen (ext)] = '\0';

    /* Form a new filename and try to open that one. */
    name = assemble_filename (path, root, uc_ext);
    rfree (uc_ext);
    f = fopen (name, how);
  }
#endif
#endif
  rfree(name);
  return f;
}


static genfile findread(file_context_rec *fc, filetype ft)
{
  genfile f;

  f=NULL;

#ifdef UNIX
  if (fc->special) {  /* It's a pipe */
    f=try_open_pipe(fc,ft,0);
    return f;
  }
#endif
  if (ft==fAGT_STD) {
    f=try_open_file(fc->path,AGTpSTD,"",filetype_info(ft,0),0);
    return f;
  }
  if (ft==fAGX || ft==fNONE)  /* Try opening w/o added extension */
    f=try_open_file(fc->path,fc->shortname,fc->ext,filetype_info(ft,0),0);
  if (f==NULL)
    f=try_open_file(fc->path,fc->shortname,extname[ft],filetype_info(ft,0),0);
  return f;
}


/*----------------------------------------------------------------------*/
/*  File IO Routines                                                    */
/*----------------------------------------------------------------------*/

genfile readopen(fc_type fc, filetype ft, char **errstr)
{
  genfile f;

  *errstr=NULL;
  f=findread(fc,ft);
  if (f==NULL) {
    const char *s, *t;
#ifdef UNIX
    if (errno==0 && fc->special) { /* Bad pipe type */
      *errstr=rstrdup("Invalid pipe request.");
      return f;
    }
#endif    
    s=strerror(errno);
    t=formal_name(fc,ft);
    *errstr=rmalloc(30+strlen(t)+strlen(s));
    sprintf(*errstr,"Cannot open file %s: %s.",t,s);
  }
  return f;
}

rbool fileexist(fc_type fc, filetype ft)
{
  genfile f;

  if (fc->special) return 0;
  f=try_open_file(fc->path,fc->shortname,extname[ft],filetype_info(ft,0),1);
  if (f!=NULL) { /* File already exists */
    readclose(f);
    return 1;
  }
  return 0;
}


genfile writeopen(fc_type fc, filetype ft, 
		  file_id_type *pfileid, char **errstr)
{
  char *name;
  genfile f;

  *errstr=NULL;
  name=NULL;

#ifdef UNIX
  if (fc->special) {  /* It's a pipe */
    f=try_open_pipe(fc,ft,1);
    if (f==NULL && errno==0) {
      *errstr=rstrdup("Invalid pipe request.");
      return f;
    }
    if (f==NULL)  /* For error messages */
      name=rstrdup(fc->gamename);
  } else
#endif
    {
      name=assemble_filename(FC(fc)->path,FC(fc)->shortname,extname[ft]);
      f=fopen(name,filetype_info(ft,1));
    }
  if (f==NULL) {
    const char *s;
    s=strerror(errno);
    *errstr=rmalloc(30+strlen(name)+strlen(s));
    sprintf(*errstr,"Cannot open file %s: %s.",name,s);
  }
  if (pfileid==NULL)
    rfree(name);
  else 
    *pfileid=(void*)name;
  return f;
}


rbool filevalid(genfile f, filetype ft)
{
  return (f!=NULL);
}



void binseek(genfile f, long offset)
{
  assert(f!=NULL);
  assert(offset>=0);
#ifdef UNIX_IO
  if (lseek(fileno(f),offset,SEEK_SET)==-1) 
#else
  if (fseek(f,offset,SEEK_SET)!=0) 
#endif
    fatal(strerror(errno));
}


/* This returns the number of bytes read, or 0 if there was an error. */
long varread(genfile f, void *buff, long recsize, long recnum, char **errstr)
{
  long num;

  *errstr=NULL;
  assert(f!=NULL);
#ifdef UNIX_IO
#ifdef MSDOS16
  num=(unsigned int)read(fileno(f),buff,recsize*recnum);
  if (num==(unsigned int)-1) 
#else
    num=read(fileno(f),buff,recsize*recnum);
    if (num==-1) 
#endif
    {
      *errstr=rstrdup(strerror(errno));
      return 0;
    }
#else
    errno=0;
    num=fread(buff,recsize,recnum,f);
    if (num!=recnum && errno!=0)
      *errstr=rstrdup(strerror(errno));
    num=num*recsize;
#endif
    return num;
}

rbool binread(genfile f, void *buff, long recsize, long recnum, char **errstr)
{
  long num;

  num=varread(f,buff,recsize,recnum,errstr);
  if (num<recsize*recnum && *errstr==NULL) 
    *errstr=rstrdup("Unexpected end of file.");
  return (*errstr==NULL);
}


rbool binwrite(genfile f, void *buff, long recsize, long recnum, rbool ferr)
{
  assert(f!=NULL);
#ifdef UNIX_IO
  if (write(fileno(f),buff,recsize*recnum)==-1)
#else
  if (fwrite(buff,recsize,recnum,f)!=recnum)
#endif
    {
    if (ferr) fatal(strerror(errno));
    return 0;
  }
  return 1;
}

#ifdef UNIX 
static rbool closepipe(genfile f)
{
  int i;
  for(i=0;i<pipecnt;i++) 
    if (pipelist[i]==f) {
      pclose(f);
      for(;i<pipecnt-1;i++) 
	pipelist[i]=pipelist[i+1];
      pipecnt--;
      return 1;
    }
  return 0;
}
#endif

void readclose(genfile f)
{
  assert(f!=NULL);
#ifdef UNIX
  if (closepipe(f)) return;
#endif
  if (fclose(f)==EOF)
    fatal(strerror(errno));
}

void writeclose(genfile f, file_id_type fileid)
{
  assert(f!=NULL);
  rfree(fileid);  
#ifdef UNIX
  if (closepipe(f)) return;
#endif
  fclose(f);
}

void binremove(genfile f, file_id_type fileid) 
{
  assert(f!=NULL); assert(fileid!=NULL);
  fclose(f);
  remove((char*)fileid);
  rfree(fileid);
}

long binsize(genfile f)
/* Returns the size of a binary file */
{
  long pos, leng;

  assert(f!=NULL);
#ifdef UNIX_IO
  {
    int fd;

    fd=fileno(f);
    pos=lseek(fd,0,SEEK_CUR);
    leng=lseek(fd,0,SEEK_END);
    lseek(fd,pos,SEEK_SET);
  }
#else
  pos=ftell(f);
  fseek(f,0,SEEK_END);
  leng=ftell(f);
  fseek(f,pos,SEEK_SET);
#endif
  return leng;
}

rbool textrewind(genfile f)
{
  assert(f!=NULL);
  rewind(f);
  return 0;
}


genfile badfile(filetype ft)
{
  return NULL;
}

/* {{{ Copyright */

/***********************************************************
Copyright 1991-1995 by Stichting Mathematisch Centrum, Amsterdam,
The Netherlands.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the names of Stichting Mathematisch
Centrum or CWI or Corporation for National Research Initiatives or
CNRI not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

While CWI is the initial source for this software, a modified version
is made available by the Corporation for National Research Initiatives
(CNRI) at the Internet address ftp://ftp.python.org.

STICHTING MATHEMATISCH CENTRUM AND CNRI DISCLAIM ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL STICHTING MATHEMATISCH
CENTRUM OR CNRI BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

******************************************************************/

/* }}} */

/* $Id: Zephyr.c,v 6.9 2005/08/14 13:56:08 geek Exp $ */

#ifndef __GNUC__
#error "Must be compiled with gcc!"
#endif

#include "Python.h"
#include "object.h"
#include <zephyr/zephyr.h>
#ifndef __FreeBSD__
#include <alloca.h>
#endif /* __FreeBSD__ */
#include <com_err.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <dictobject.h>

#ifndef STUPID_ZEPHYR_HEADER_FILE
#define STUPID_ZEPHYR_HEADER_FILE
Code_t ZGetLocations(ZLocations_t location[], int *numloc);
Code_t ZUnsetLocation(void);
Code_t ZSetLocation(char *exposure);
Code_t ZGetSubscriptions(ZSubscription_t *, int*);
#endif


/* -------------------------------------------------------- */
/* {{{ Struct declarations and global variables */

static PyObject *ErrorObject;

static PyObject *z_packet;
static PyObject *z_version;
static PyObject *z_kind;
static PyObject *z_uid;
static PyObject *z_time;
static PyObject *z_port;
static PyObject *z_auth;
static PyObject *z_checked_auth;
static PyObject *z_authent_len;
static PyObject *z_ascii_authent;
static PyObject *z_class;
static PyObject *z_class_inst;
static PyObject *z_opcode;
static PyObject *z_sender;
static PyObject *z_recipient;
static PyObject *z_default_format;
/*  static PyObject *z_multinotice; *//* NOT NEEDED */
/*  static PyObject *z_multiuid; *//* NOT NEEDED */
/*  static PyObject *z_checksum; *//* NOT NEEDED */
static PyObject *z_num_other_fields;
static PyObject *z_other_fields;
/*  static PyObject *z_num_other_fields; *//* NOT NEEDED */

static PyObject *signature;
static PyObject *message;
static PyObject *default_realm;
static PyObject *from_host;
static PyObject *zephyr;

static char *z_kinds[] = {
  "UNSAFE",
  "UNACKED",
  "ACKED",
  "HMACK",
  "HMCTL",
  "SERVACK",
  "SERVNAK",
  "CLIENTACK",
  "STAT"
};

static char *DEFAULT_FORMAT = "Class $class, Instance $instance:\nTo: @bold($recipient) at $time $date\nFrom: @bold($1) <$sender>\n\n$2";

/* ---------------------------------------------------------------- */

/* Declarations for objects of type Zephyr */

typedef struct {
  PyObject_HEAD
  /* XXXX Add your own stuff here */
  unsigned short port;
  Z_AuthProc authentic;
  int blocking;
} Zephyrobject;

staticforward PyTypeObject Zephyrtype;
static Zephyrobject* SingleZephyr = NULL;

/* ---------------------------------------------------------------- */

/* }}} */
/* -------------------------------------------------------- */
/* {{{ Utility routines */

/* {{{ char *ipaddr(struct in_addr) */

static char *ipaddr(struct in_addr inaddr) /* return a character hostname for ip address inaddr */
{
  static struct hostent *h;
  static char str[20];
  unsigned long x;
  
  x = inaddr.s_addr;
  h = (struct hostent *) gethostbyaddr((char*)&(inaddr), sizeof(struct in_addr), 
				       AF_INET);
  if (h && h->h_name) {
    return(h->h_name);
        }
  else {
    sprintf(str, "%ld.%ld.%ld.%ld", (x&0xff000000)>>24, (x&0x00ff0000)>> 16, (x&0x0000ff00)>>8, (x&0x000000ff));
    return(str);
  }
}

/* }}} */

/* Returns a brand-spanking new tuple
   This routine also uses the Python convention that returning NULL is
   bad.*/
PyObject *sub_to_tuple(ZSubscription_t *sub)
{
  PyObject *v;
  int len=0;

  len = strlen(sub->zsub_recipient); 
  if (len) {
    v = Py_BuildValue("(sss)", sub->zsub_class, sub->zsub_classinst, sub->zsub_recipient);
  } else {
    v = Py_BuildValue("(sss)", sub->zsub_class, sub->zsub_classinst, "*");
  }
  return v;
}

/* }}} */
/* -------------------------------------------------------- */
/* {{{ Zephyr_send() and friends */

/* {{{ void* GetDictItemWithDefault(PyObject *, PyObject *, char*) */

static void* GetDictItemWithDefault(PyObject *dict, PyObject *object, char* deflt) {
  PyObject *di;
  char *s;
  
  di = PyDict_GetItem(dict, object);
  if (!di) return deflt;
  if (di != Py_None) {
    s = PyString_AsString(di);
    if (strlen(s)) {
      return s;
    }
  } else {
    Py_DECREF(di);		/* Don't leak! */
  }
  
  return deflt;
}

/* }}} */
/* {{{ ZNotice_t *dict_to_zg(PyObject *dict) */

/* This is used to convert a Python dictionary into a sendable */
/* zephyrgram.  Some of the fields will be ignored, because they are */
/* filled in automatically.  For this to work, realloc() must work. */

#define ZBUFFSIZE 2048
#define SIZECHECK(s1,s2,ptr) while (s1 >= s2) {realloc(ptr, s2 + ZBUFFSIZE);s2 += ZBUFFSIZE;}

static int append_data(char **buf, PyObject *dict, PyObject *find, char *def, 
		       int offset, int *bufleft) {
  char *s = GetDictItemWithDefault(dict, find, def);
  int sz;
  if (s) {
    sz = strlen(s) + 1;  
    SIZECHECK(sz, (*bufleft), (*buf));
    memcpy((*buf)+offset, s, sz);
    *bufleft -= sz;
    return sz;
  }
  return 0;
}

static void handle_other_fields(char **buf, PyObject *dict,
				int offset, int *bufleft)
{
  int sz, i, needed;
  char *s;
  ZNotice_t *notice;
  PyObject *p, *q, *t = PyDict_GetItem(dict, z_other_fields);
  if (!t) return;		/* Some error.  Probably should handle this */
  while (1) {			/* make cleanup easy/standard */
    notice = (ZNotice_t*)(*buf);
    if (t == Py_None) break;	/* Not found, clean up and return */
    if (!PySequence_Check(t)) break; /* make sure it's a sequence... */
  
    sz = PyObject_Length(t);
    if (sz < 1) break;
    notice->z_num_other_fields = sz;
    for(i = 0, needed = 1; i < sz; i++, needed = 1) {
      notice->z_other_fields[i] = (*buf) + offset;
      p = PySequence_GetItem(t, i); /* borrow a ref! */
      if (p) {
	if (p != Py_None) {
	  q = PyObject_Str(p);
	} else {
	  q = NULL;
	}
	if (q) {
	  s = PyString_AsString(q);
	  Py_DECREF(q);
	} else {
	  s = NULL;
	}
	if (s) {
	  needed += strlen(s);
	}
      }
      SIZECHECK(needed, (*bufleft), (*buf));
      memcpy((*buf)+offset, s, needed - 1);
      notice->z_other_fields[i][needed - 1] = (char)NULL;
      *bufleft -= needed;
      offset += needed;
    }
    break;
  }
  return;
}


static ZNotice_t *dict_to_zg(PyObject *dict)
{
  char *buf;
  int sz = 0, zsize = 0, offset;
  ZNotice_t* notice;
  
  buf = calloc(1, ZBUFFSIZE);	/* Allocate initial buffer. */
  if (!buf) {
    PyErr_SetString(ErrorObject, "Out of Memory");
    return NULL;
  }
  
  notice = (ZNotice_t*)buf;
  notice->z_message_len = 0;
  notice->z_kind = ACKED;

  offset = sizeof(ZNotice_t);
  notice->z_message = buf + offset;
  zsize = ZBUFFSIZE - offset;
  
  sz = append_data(&buf, dict, signature, "", offset, &zsize);
  offset += sz;
  notice->z_message_len += sz;
  sz = append_data(&buf, dict, message, "", offset, &zsize);
  if (!sz) {
    PyErr_SetString(ErrorObject, "message not specified!");
    return NULL;
  } else {
    offset += sz;
    notice->z_message_len += sz;
  }
  
  sz = append_data(&buf, dict, z_class, "MESSAGE", offset, &zsize);
  notice->z_class = buf + offset;
  offset += sz;

  sz = append_data(&buf, dict, z_class_inst, "", offset, &zsize);
  notice->z_class_inst = buf + offset;
  offset += sz;

  sz = append_data(&buf, dict, z_default_format, DEFAULT_FORMAT,offset,&zsize);
  notice->z_default_format = buf + offset;
  offset += sz;

  sz = append_data(&buf, dict, z_opcode, "", offset, &zsize);
  notice->z_opcode = buf + offset;
  offset += sz;

  sz = append_data(&buf, dict, z_recipient, "", offset, &zsize);
  notice->z_recipient = buf + offset;
  offset += sz;
  handle_other_fields(&buf, dict, offset, &zsize);
  return notice;
}

/* }}} */

static char Zephyr_send__doc__[] = "\n\
Sends a zephyrgram.\n\
\n\
This function takes a dictionary as an argument.  The fields are extracted by\n\
name and stuffed into a new ZNotice structure.  Minimally, the dictionary must\n\
include members for 'recipient' and 'message'.  Everything else has a (possibly\n\
ugly) default.\n\
\n\
Keys: recipient, message, signature, zclass, instance, opcode, default_format"
;

static PyObject *Zephyr_send(Zephyrobject *self, PyObject *args)
{
  int retval = ZERR_NONE;
  ZNotice_t *notice = NULL;
  PyObject *dict = NULL;
  
  if (!PyArg_ParseTuple(args, "O", &dict))
    return NULL;

  if (!PyDict_Check(dict)) {
    PyErr_SetString(ErrorObject, "Send requires a dictionary");
    return NULL;
  }
  
  notice = dict_to_zg(dict); /* Copy rest from dict */
  if (!notice) {
    return NULL;
  }
  
  retval = ZSendNotice(notice, self->authentic);
  free(notice);
  if (retval != ZERR_NONE) {
    PyErr_SetString(ErrorObject, error_message(retval));
    return NULL;
  }  
  Py_INCREF(Py_None);
  return Py_None;
}

/* }}} */
/* {{{ Zephyr_check() */

static void SetDictHandleNull(PyObject *dict, PyObject *key, char *s) {
  PyObject *p;

  if (s) {
    p = PyString_FromString(s);
    if (p) {
      PyDict_SetItem(dict, key, p);
      Py_DECREF(p);
      return;
    }
  }
  PyDict_SetItem(dict, key, Py_None);
}

static PyObject * zg_to_dict(ZNotice_t *notice)
{
  int c;
  char *t;
  PyObject *p, *dict, *q;
  char *buffer;

  dict = PyDict_New();
  
  SetDictHandleNull(dict, z_packet, notice->z_packet);
  SetDictHandleNull(dict, z_version, notice->z_version);
				/* z_kind handled below */
  p = PyTuple_New(3);
  if (!p) return NULL;
  q = PyLong_FromUnsignedLong(notice->z_uid.zuid_addr.s_addr);
  if (!q) return NULL;
  PyTuple_SET_ITEM(p, 0, q);

  q = PyInt_FromLong(notice->z_uid.tv.tv_sec);
  if (!q) return NULL;
  PyTuple_SET_ITEM(p, 1, q);

  q = PyInt_FromLong(notice->z_uid.tv.tv_usec);
  if (!q) return NULL;
  PyTuple_SET_ITEM(p, 2, q);
  PyDict_SetItem(dict, z_uid, p);
  Py_DECREF(p);
  
				/* z_time handled below */
  p = PyInt_FromLong(notice->z_port);
  PyDict_SetItem(dict, z_port, p);
  Py_DECREF(p);
				/* z_auth handled below */
  p = PyInt_FromLong(notice->z_checked_auth);
  PyDict_SetItem(dict, z_checked_auth, p);
  Py_DECREF(p);
  p = PyInt_FromLong(notice->z_authent_len);
  PyDict_SetItem(dict, z_authent_len, p);
  Py_DECREF(p);
  SetDictHandleNull(dict, z_ascii_authent, notice->z_ascii_authent);
  SetDictHandleNull(dict, z_class, notice->z_class);
  SetDictHandleNull(dict, z_class_inst, notice->z_class_inst);
  SetDictHandleNull(dict, z_opcode, notice->z_opcode);
  SetDictHandleNull(dict, z_sender, notice->z_sender);
				/* z_recipient handled below */
  SetDictHandleNull(dict, z_default_format, notice->z_default_format);

  SetDictHandleNull(dict, from_host, ipaddr(notice->z_sender_addr));

  p = PyInt_FromLong((long)notice->z_num_other_fields);
  PyDict_SetItem(dict, z_num_other_fields, p);
  Py_DECREF(p);
  
  p = PyInt_FromLong((long)notice->z_kind);
  PyDict_SetItem(dict, z_kind, p);
  Py_DECREF(p);
  
  t = asctime(localtime(&(notice->z_time.tv_sec)));
  p =  PyString_FromStringAndSize(t, strlen(t) - 1);
  PyDict_SetItem(dict, z_time, p);
  Py_DECREF(p);

  p = PyInt_FromLong((long)(notice->z_auth));
  PyDict_SetItem(dict, z_auth, p);
  Py_DECREF(p);
  
  if (strlen(notice->z_recipient)) {
    SetDictHandleNull(dict, z_recipient, notice->z_recipient);
  } else {
    SetDictHandleNull(dict, z_recipient, "*");
  }

  if (notice->z_message_len > 0) {
    SetDictHandleNull(dict, signature, notice->z_message);
    c = strlen(notice->z_message) + 1; /* Length of signature */
    t = notice->z_message + c;	/* Pointer to message body */
    if (c >= notice->z_message_len) { /* Signature IS body */
      p = PyString_FromString("Invalid zephyrgram: no body");
    } else {			/* Grab both sig and body */
      buffer = strndup(t, (notice->z_message_len) - c);
      p = PyString_FromString(buffer);
      free(buffer);
    }
    PyDict_SetItem(dict, message, p);
    Py_DECREF(p);
  } else {
    SetDictHandleNull(dict, signature, "");
    SetDictHandleNull(dict, message, "");
  }
  
  if (notice->z_num_other_fields) {
    q = PyList_New(0);
    for(c = 0; c < notice->z_num_other_fields; c++) {
      p = PyString_FromString((notice->z_other_fields)[c]);
      PyList_Append(q, p);
      Py_DECREF(p);
    }
    PyDict_SetItem(dict, z_other_fields, q);
    Py_DECREF(q);
  } else {
    PyDict_SetItem(dict, z_other_fields, Py_None);
  }
  
  return dict;
}


static char Zephyr_check__doc__[] = 
"Checks the input queue for the current subscription list.  This will\n\
return either None or a zephyrgram.  Programmatically, you should call\n\
this repeatedly until you get None, and then go do something else for\n\
a while.\n\
\n\
It will raise an exception if there is an internal Zephyr error."
;

static PyObject *
Zephyr_check(Zephyrobject *self, PyObject *args)
{
  int result, len;
  PyObject *p;
  ZNotice_t *notice;
  struct sockaddr_in from;
  
  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  if (!self->blocking) {	/* If blocking, skip exit checks */
    len = ZPending();		/* Check queue */
    if (len < 0) {
      PyErr_SetString(ErrorObject, strerror(errno));
      return NULL;
    }
    
    if (!len){			/* Nothing there */
      Py_INCREF(Py_None);
      return Py_None;
    }
  }

  notice = calloc(1, sizeof(ZNotice_t));
  if (!notice) {
    PyErr_SetString(ErrorObject, strerror(errno));
    return NULL;
  }
  result = ZReceiveNotice(notice, &from); /* Should this be port specific? */
  if (result != ZERR_NONE) {
    PyErr_SetString(ErrorObject, error_message(result));
    return NULL;
  }
  p = zg_to_dict(notice);
  PyDict_SetItem(p, zephyr, (PyObject*)self);
  ZFreeNotice(notice);
  return p;
}

/* }}} */
/* {{{ subscription stuff */

static PyObject *build_subscription_list(Zephyrobject *self) {
  PyObject *l, *v;
  int totalsubs, result, i, numsubs = 1;  
  ZSubscription_t subscription;

  memset(&subscription, 0, sizeof(ZSubscription_t)); /* A clean slate! */
    
  if (!(l = PyList_New(0))) {	/* Get new, empty list */
    return NULL;
  }

  if ((result = ZRetrieveSubscriptions(self->port, &totalsubs)) != ZERR_NONE) {
    PyErr_SetString(ErrorObject, error_message(result));
    return NULL;
  }
  
  for (i = 0; i < totalsubs; i++, numsubs = 1) {
    if ((result = ZGetSubscriptions(&subscription, &numsubs)) != ZERR_NONE) {
      PyErr_SetString(ErrorObject, error_message(result));
      return NULL;
    }
    v = PyTuple_New(3);		/* Get a new tuple */
    if (!v) return NULL;
    PyTuple_SET_ITEM(v, 0, PyString_FromString(subscription.zsub_class));
    PyTuple_SET_ITEM(v, 1, PyString_FromString(subscription.zsub_classinst));
    PyTuple_SET_ITEM(v, 2, PyString_FromString(subscription.zsub_recipient));
    PyList_Append(l, v);
    Py_DECREF(v);		/* Throw away our reference to it */
  }
  return l;
}


static char Zephyr_subscribe__doc__[] = 
"Subscribes to an optional (class,instance,recipient) triple.\n\
If no argument is passed return the current subscription list.";

static PyObject *Zephyr_subscribe(Zephyrobject *self, PyObject *args) {
  PyObject *o = NULL;
  ZSubscription_t subscription;

  memset(&subscription, 0, sizeof(ZSubscription_t)); /* A clean slate! */
  if (!PyArg_ParseTuple(args, "|O",&o)) {
    PyErr_SetString(ErrorObject,
		    "Must pass either nothing or a tuple");
    return NULL;
  }

  if (o) {
    if (!PyArg_ParseTuple(o, "ss|s", 
			  &(subscription.zsub_class),
			  &(subscription.zsub_classinst), 
			  &(subscription.zsub_recipient))) {
      PyErr_SetString(ErrorObject,
		      "Tuple must be (class, instance [,recipient]).");
      return NULL;
    }  
    ZSubscribeTo(&subscription, 1, self->port);
    Py_INCREF(Py_None);
    return Py_None;

  }
  return build_subscription_list(self);
}

static char Zephyr_unsubscribe__doc__[] = 
"Remove a subscription, returns the remaining subscription list."
;

static PyObject *
Zephyr_unsubscribe(Zephyrobject *self, PyObject *args)
{
  int retval;
  PyObject *o = NULL;
  ZSubscription_t sub;
    
  memset(&sub, 0, sizeof(ZSubscription_t)); /* Start with a clean slate! */

  if (!(PyArg_ParseTuple(args, "O", &o))) {
    PyErr_SetString(ErrorObject,
		    "Must use a triple");
    return NULL;
  }
  
  retval = PyArg_ParseTuple(o, "sss", 
			    &(sub.zsub_class),
			    &(sub.zsub_classinst), 
			    &(sub.zsub_recipient));
  ZUnsubscribeTo(&sub, 1, 0);
  Py_INCREF(Py_None);
  return build_subscription_list(self);
}


static char Zephyr_cancel_sub__doc__[] = 
"Cancel all existing subscriptions."
;

static PyObject *Zephyr_cancel_sub(Zephyrobject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  ZCancelSubscriptions(0);
  Py_INCREF(Py_None);
  return Py_None;
}

/* }}} */
/* {{{ Zephyr_setblocking(Zephyrobject, PyObject) */

static char Zephyr_setblocking__doc__[] = 
"Set to 0 (default) to make check() return immediately."
;

static PyObject *
Zephyr_setblocking(Zephyrobject *self, PyObject *args)
{
  int t = self->blocking;
  
  if (!PyArg_ParseTuple(args, "|i", &t))
    return NULL;
  self->blocking = t;
  
  return PyInt_FromLong((long)t);
}

/* }}} */
/* {{{ Zephyr_authenticate(Zephyrobject *, PyObject *) */

static char Zephyr_authenticate__doc__[] = 
"Turn authentication on or off.  A parameter of 0 (zero) turns it off,\n\
while a parameter of 1 turns it on.  If no parameter is passed, it\n\
assumes that you just want the current setting."
;

static PyObject *
Zephyr_authenticate(Zephyrobject *self, PyObject *args)
{
  int val = -1;
  
  if (!PyArg_ParseTuple(args, "|i", &val))
    return NULL;
  switch (val) {
  case 0:
    self->authentic = ZNOAUTH;
    break;
  case 1:
    self->authentic = ZAUTH;
    break;
  }
  if (self->authentic == ZNOAUTH)
    return PyInt_FromLong(0L);
  else
    return PyInt_FromLong(1L);
}

/* }}} */
/* {{{ Zephyr_find(Zephyrobject, PyObject) */

static char Zephyr_find__doc__[] = 
"find(user [,multiple]):\n\tuser: the kerberos principal of interest.  If there"
"\n\t\tis no realm, the current realm is automatically appended."
"\n\tmultiple: If non-zero, return multiple locations\n"
;

static PyObject *
Zephyr_find(Zephyrobject *self, PyObject *args) {
  int j, result, nlocs=-1, one=1, multiple=0;
  PyObject *reslist, *t, *v, *s;
  ZLocations_t location;
  
  if (!PyArg_ParseTuple(args, "O|i", &v, &multiple))
    return NULL;
  v = PyObject_Str(v);		/* Coerce to PyString */
  if (!v) return NULL;
  s = PyString_FromString(PyString_AsString(v)); /* Copy */
    
  if (!strchr(PyString_AsString(s), '@')) { /* Force realm */
    PyString_Concat(&s, default_realm);
  }
  
  result = ZLocateUser(PyString_AsString(s), &nlocs, ZAUTH); /* Find user */
  Py_DECREF(s);
  Py_DECREF(v);
  if (result) { 
    PyErr_SetString(ErrorObject, "Error locating user");
    return NULL;
  }

  if (nlocs && !multiple) nlocs = 1; /* Limit to a single location */  
  reslist = PyList_New(0);	/* We'll always return a list... */
  for(j = 0; j < nlocs; j++) {
    result = ZGetLocations(&location, &one);
    if (result) {
      PyErr_SetString(ErrorObject, "Error getting user location");
      return NULL;
    }
    t = Py_BuildValue("(sss)", location.host, location.time, location.tty);
    PyList_Append(reslist, t);
  }
  Py_INCREF(reslist);
  return reslist;
}

/* }}} */
/* {{{ Zephyr_expose(Zephyrobject, Pyobject) */

static char Zephyr_expose__doc__[] = 
"Set your Zephyr exposure level."
;

static PyObject *
Zephyr_expose(Zephyrobject *self, PyObject *args)
{
  char *exposure;
  
  if (!PyArg_ParseTuple(args, "s", &exposure))
    return NULL;

  if (!strcmp(exposure, EXPOSE_NONE)) {    
    ZUnsetLocation();
  } else {
    ZSetLocation(exposure);
  }  
  
  Py_INCREF(Py_None);
  return Py_None;
}

/* }}} */
/* {{{ Zephyr_flush(Zephyrobject *, PyObject *) */

static char Zephyr_flush__doc__[] = 
"Flush all Zephyr location information about you from the server\n\
database."
;

static PyObject *Zephyr_flush(Zephyrobject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  ZUnsetLocation();
  ZFlushMyLocations();
  Py_INCREF(Py_None);
  return Py_None;
}

/* }}} */
/* {{{ Zephyr_flush(Zephyrobject *, PyObject *) */

static char Zephyr_sender__doc__[] = 
"Get your name."
;

static PyObject *Zephyr_sender(Zephyrobject *self, PyObject *args)
{
	return PyString_FromString((char *) ZGetSender());
}

/* {{{ Zephyr_getfd */

static char Zephyr_getfd__doc__[] = 
"Get a file descriptor that can be used with select."
;

static PyObject *Zephyr_getfd(Zephyrobject *self, PyObject *args)
{
  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  return PyInt_FromLong((long) ZGetFD());
}

/* }}} */
/* {{{ housekeeping */

static struct PyMethodDef Zephyr_methods[] = {
  {"check",
   (PyCFunction)Zephyr_check,	METH_VARARGS,	Zephyr_check__doc__},
  {"cancel",
   (PyCFunction)Zephyr_cancel_sub, METH_VARARGS, Zephyr_cancel_sub__doc__},
  {"setblocking",
   (PyCFunction)Zephyr_setblocking, METH_VARARGS, Zephyr_setblocking__doc__},
  {"authenticate",
   (PyCFunction)Zephyr_authenticate, METH_VARARGS, Zephyr_authenticate__doc__},
  {"find",
   (PyCFunction)Zephyr_find,	METH_VARARGS,	Zephyr_find__doc__},
  {"expose",
   (PyCFunction)Zephyr_expose,	METH_VARARGS,	Zephyr_expose__doc__},
  {"flush",
   (PyCFunction)Zephyr_flush,	METH_VARARGS,	Zephyr_flush__doc__},
  {"getfd",
   (PyCFunction)Zephyr_getfd,	METH_VARARGS,	Zephyr_getfd__doc__},
  {"send",
   (PyCFunction)Zephyr_send,	METH_VARARGS,	Zephyr_send__doc__},
  {"subscribe",
   (PyCFunction)Zephyr_subscribe, METH_VARARGS,	Zephyr_subscribe__doc__},
  {"unsubscribe",
   (PyCFunction)Zephyr_unsubscribe, METH_VARARGS, Zephyr_unsubscribe__doc__},
  {"sender",
   (PyCFunction)Zephyr_sender, METH_VARARGS, Zephyr_sender__doc__},
  {NULL,		NULL}		/* sentinel */
};

/* ---------- */

static void Zephyr_dealloc(Zephyrobject *self)
{
  /**
  PyMem_DEL(self);
  SingleZephyr = (Zephyrobject *)NULL;
  /**/
}

static Zephyrobject *newZephyrobject(void)
{
  int retval;

  if (SingleZephyr) {
    Py_INCREF(SingleZephyr);
    return SingleZephyr;
  }
  fflush(stdout);
  SingleZephyr = PyObject_NEW(Zephyrobject, &Zephyrtype);
  if (SingleZephyr == NULL)
    return NULL;
  SingleZephyr->authentic = ZAUTH;
  SingleZephyr->port = 0;
  SingleZephyr->blocking = 0;

  retval = ZOpenPort(&(SingleZephyr->port));
  if (retval != ZERR_NONE) {
    com_err("Zephyr.c", retval, "can't open a port");fflush(stderr);
    PyErr_SetString(ErrorObject, error_message(retval));
    Zephyr_dealloc(SingleZephyr);
    return NULL;
  }
  return SingleZephyr;
}


static PyObject *Zephyr_getattr(Zephyrobject *self, char *name)
{
  return Py_FindMethod(Zephyr_methods, (PyObject *)self, name);
}

static char Zephyrtype__doc__[] = 
"A Zephyr object will create ZephyrGrams or Subscriptions."
;

statichere PyTypeObject Zephyrtype = {
	PyObject_HEAD_INIT(NULL)
	0,				/*ob_size*/
	"Zephyr",			/*tp_name*/
	sizeof(Zephyrobject),		/*tp_basicsize*/
	0,				/*tp_itemsize*/
	/* methods */
	(destructor)Zephyr_dealloc,	/*tp_dealloc*/
	(printfunc)0,			/*tp_print*/
	(getattrfunc)Zephyr_getattr,	/*tp_getattr*/
	(setattrfunc)0,			/*tp_setattr*/
	(cmpfunc)0,			/*tp_compare*/
	(reprfunc)0,			/*tp_repr*/
	0,				/*tp_as_number*/
	0,				/*tp_as_sequence*/
	0,				/*tp_as_mapping*/
	(hashfunc)0,			/*tp_hash*/
	(ternaryfunc)0,			/*tp_call*/
	(reprfunc)0,			/*tp_str*/
	/* Space for future expansion */
	0L,0L,0L,0L,
	Zephyrtype__doc__		/* Documentation string */
};

/* }}} */

/* -------------------------------------------------------- */
/* {{{ Module-level code */

static char Zephyr_new_zephyr__doc__[] =
"Create a Zephyr object"
;

static PyObject *
Zephyr_new_zephyr(PyObject *self /*Not used here*/, 
PyObject *args)
{
  Zephyrobject *z;
  
  if (!PyArg_ParseTuple(args, ""))
    return NULL;
  z = newZephyrobject();
  if (z)
    return (PyObject*)z;
  
  return (PyObject*)NULL;	/* PyErr should already be set. */
}

/* List of methods defined in the module */

static struct PyMethodDef z_methods[] = {
  {"Zephyr",	(PyCFunction)Zephyr_new_zephyr,	METH_VARARGS,	Zephyr_new_zephyr__doc__},
  
  {NULL,	 (PyCFunction)NULL, 0, NULL}		/* sentinel */
};


/* Initialization function for the module (*must* be called initZephyr) */

static char Zephyr_module_documentation[] = 
"Objects and routines for interacting with Zephyr."
;

void
initZephyr(void)
{
  PyObject *m, *d;
  int retval;
  char rlm[40];
  
  /* Create the module and add the functions */
  m = Py_InitModule4("Zephyr", z_methods,
		     Zephyr_module_documentation,
		     (PyObject*)NULL,PYTHON_API_VERSION);


  /* Add some symbolic constants to the module */
  d = PyModule_GetDict(m);
  ErrorObject = PyString_FromString("Zephyr.error");
  PyDict_SetItemString(d, "error", ErrorObject);

  PyDict_SetItemString(d, "EXPOSE_NONE",	PyString_FromString(EXPOSE_NONE));
  PyDict_SetItemString(d, "EXPOSE_OPSTAFF",	PyString_FromString(EXPOSE_OPSTAFF));
  PyDict_SetItemString(d, "EXPOSE_REALMVIS",	PyString_FromString(EXPOSE_REALMVIS));
  PyDict_SetItemString(d, "EXPOSE_REALMANN",	PyString_FromString(EXPOSE_REALMANN));
  PyDict_SetItemString(d, "EXPOSE_NETVIS",	PyString_FromString(EXPOSE_NETVIS));
  PyDict_SetItemString(d, "EXPOSE_NETANN",	PyString_FromString(EXPOSE_NETANN));
  PyDict_SetItemString(d, z_kinds[UNSAFE],	PyInt_FromLong((long)UNSAFE));
  PyDict_SetItemString(d, z_kinds[UNACKED],	PyInt_FromLong((long)UNACKED));
  PyDict_SetItemString(d, z_kinds[ACKED],	PyInt_FromLong((long)ACKED));
  PyDict_SetItemString(d, z_kinds[HMACK],	PyInt_FromLong((long)HMACK));
  PyDict_SetItemString(d, z_kinds[HMCTL],	PyInt_FromLong((long)HMCTL));
  PyDict_SetItemString(d, z_kinds[SERVACK],	PyInt_FromLong((long)SERVACK));
  PyDict_SetItemString(d, z_kinds[SERVNAK],	PyInt_FromLong((long)SERVNAK));
  PyDict_SetItemString(d, z_kinds[CLIENTACK],	PyInt_FromLong((long)CLIENTACK));
  PyDict_SetItemString(d, z_kinds[STAT],	PyInt_FromLong((long)STAT));

  /* XXXX Add constants here */
  retval = ZInitialize();
  if (retval != ZERR_NONE) {
    fprintf(stderr, "can't initialize module Zephyr");fflush(stderr);
    Py_FatalError(strdup(error_message(retval))); /* Don't worry about lost memory here! */
  }

  /* Magic string constants */
  z_packet = PyString_FromString("packet");
  PyDict_SetItem(d, z_packet, z_packet);
  z_version = PyString_FromString("version");
  PyDict_SetItem(d, z_version, z_version);
  z_kind = PyString_FromString("kind");
  PyDict_SetItem(d, z_kind, z_kind);
  z_uid = PyString_FromString("uid");
  PyDict_SetItem(d, z_uid, z_uid);
  z_time = PyString_FromString("time");
  PyDict_SetItem(d, z_time, z_time);
  z_port = PyString_FromString("port");
  PyDict_SetItem(d, z_port, z_port);
  z_auth = PyString_FromString("auth");
  PyDict_SetItem(d, z_auth, z_auth);
  z_checked_auth = PyString_FromString("checked_auth");
  PyDict_SetItem(d, z_checked_auth, z_checked_auth);
  z_authent_len = PyString_FromString("authent_len");
  PyDict_SetItem(d, z_authent_len, z_authent_len);
  z_ascii_authent = PyString_FromString("ascii_authent");
  PyDict_SetItem(d, z_ascii_authent, z_ascii_authent);
  z_class = PyString_FromString("zclass");
  PyDict_SetItem(d, z_class, z_class);
  z_class_inst = PyString_FromString("instance");
  PyDict_SetItem(d, z_class_inst, z_class_inst);
  z_opcode = PyString_FromString("opcode");
  PyDict_SetItem(d, z_opcode, z_opcode);
  z_sender = PyString_FromString("sender");
  PyDict_SetItem(d, z_sender, z_sender);
  z_recipient = PyString_FromString("recipient");
  PyDict_SetItem(d, z_recipient, z_recipient);
  z_default_format = PyString_FromString("format");
  PyDict_SetItem(d, z_default_format, z_default_format);
  z_num_other_fields = PyString_FromString("num_other_fields");
  PyDict_SetItem(d, z_num_other_fields, z_num_other_fields);
  z_other_fields = PyString_FromString("other_fields");
  PyDict_SetItem(d, z_other_fields, z_other_fields);
  signature = PyString_FromString("signature");
  PyDict_SetItem(d, signature, signature);
  message = PyString_FromString("message");
  PyDict_SetItem(d, message, message);
  zephyr = PyString_FromString("zephyr");
  PyDict_SetItem(d, zephyr, zephyr);
  from_host = PyString_FromString("from_host");
  PyDict_SetItem(d, from_host, from_host);

  rlm[0] = '@';
  rlm[1] = (char)NULL;
  strcat(rlm, ZGetRealm());
  default_realm = PyString_FromString(rlm);
  PyDict_SetItem(d, default_realm, default_realm);

  /* Check for errors */
  if (PyErr_Occurred())
    Py_FatalError("can't initialize module Zephyr");
}

/* }}} */
/* -------------------------------------------------------- */

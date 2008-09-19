/* -*- Mode: C++; Indent: Indent4 -*- */
#include "builtin.h"
#include <climits>
#include <numeric>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

namespace NAMESPACE {

class_ *cl_class_, *cl_none, *cl_str_, *cl_int_, *cl_float_, *cl_list, *cl_tuple, *cl_dict, *cl_set, *cl_object, *cl_rangeiter;

str *sp;
__GC_STRING ws, __fmtchars;
__GC_VECTOR(str *) __char_cache;


void __init() {
    GC_INIT();
#ifdef __SS_BIND
    Py_Initialize();
#endif

    cl_class_ = new class_ ("class_", 0, 0);
    cl_none = new class_("none", 1, 1);
    cl_str_ = new class_("str_", 2, 2);
    cl_int_ = new class_("int_", 3, 3);
    cl_float_ = new class_("float_", 4, 4);
    cl_list = new class_("list", 5, 5);
    cl_tuple = new class_("tuple", 6, 6);
    cl_dict = new class_("dict", 7, 7);
    cl_set = new class_("set", 8, 8);
    cl_object = new class_("object", 9, 9);
    cl_rangeiter = new class_("rangeiter", 10, 10);

    ws = " \n\r\t\f\v";
    __fmtchars = "diouxXeEfFgGhcrs%";
    sp = new str(" ");

    for(int i=0;i<256;i++) {
        char c = i;
        __char_cache.push_back(new str(&c, 1));
    }
}

double __portableround(double x) {
    if(x<0) return ceil(x-0.5);
    return floor(x+0.5);
}

/* int_ methods */

int_::int_(int i) {
    unit = i;
    __class__ = cl_int_;
}

str *int_::__repr__() {
    return __str(unit);
} 

/* float methods */

float_::float_(double f) {
    unit = f;
    __class__ = cl_float_;
}

str *float_::__repr__() {
    return __str(unit);
} 

/* str methods */

str::str() : cached_hash(0) {
    __class__ = cl_str_;
}

str::str(const char *s) : unit(s), cached_hash(0) {
    __class__ = cl_str_;
}

str::str(__GC_STRING s) : unit(s), cached_hash(0) {
    __class__ = cl_str_;
}

str::str(const char *s, int size) : unit(__GC_STRING(s, size)), cached_hash(0)  { /* '\0' delimiter in C */
    __class__ = cl_str_;
}

str *str::__getitem__(int i) {
    i = __wrap(this, i);
    return __char_cache[(unsigned char)unit[i]];
}
str *str::__getfirst__() {
    return __getitem__(0);
}
str *str::__getsecond__() {
    return __getitem__(1);
}

int str::__len__() {
    return unit.size();
}

str *str::__str__() { // weg?
    return this;
}

str *str::__repr__() {
    std::stringstream ss;
    __GC_STRING sep = "\\\n\r\t";
    __GC_STRING let = "\\nrt";

    const char *quote = "'";
    int hasq = unit.find("'");
    int hasd = unit.find("\"");

    if (hasq != -1 && hasd != -1) {
        sep += "'"; let += "'";
    }
    if (hasq != -1 && hasd == -1)
        quote = "\"";

    ss << quote;
    for(int i=0; i<(int)unit.size(); i++)
    {
        char c = unit[i];
        int k;

        if((k = sep.find_first_of(c)) != -1)
            ss << "\\" << let[k];
        else {
            int j = (int)((unsigned char)c);

            if(j<16)
                ss << "\\x0" << std::hex << j;
            else if(j>=' ' && j<='~')
                ss << (char)j;
            else
                ss << "\\x" << std::hex << j;
        }
    }
    ss << quote;

    return new str(ss.str().c_str());
}

int str::__contains__(str *s) {
    return ((int)unit.find(s->unit)) != -1;
}

int str::isspace() {
    return unit.size() && (((int)unit.find_first_not_of(ws)) == -1);
}

int str::isdigit() {
    return __ctype_function(&::isdigit);
}

int str::isalpha() {
    return __ctype_function(&::isalpha);
}

int str::isalnum() {
    return __ctype_function(&::isalnum);
}

int str::__ctype_function(int (*cfunc)(int))
{
  int i, l = unit.size();
  if(!l) 
      return 0;
  
  for(i = 0; i < l; i++) 
      if(!cfunc((int)unit[i])) return 0;
  
  return 1;
}

str *str::ljust(int width, str *s) {
    if(width<=__len__()) return this;
    if(!s) s = sp;
    return __add__(s->__mul__(width-__len__()));
}

str *str::rjust(int width, str *s) {
    if(width<=__len__()) return this;
    if(!s) s = sp;
    return s->__mul__(width-__len__())->__add__(this);
}

str *str::zfill(int width) {
    if(width<=__len__()) return this;
    return (new str("0"))->__mul__(width-__len__())->__add__(this);
}

str *str::expandtabs(int width) {
    int i;
    __GC_STRING r = unit;
    while((i = r.find("\t")) != -1)
        r.replace(i, 1, (new str(" "))->__mul__(width-i%width)->unit);
    return new str(r);
}

int str::islower() {
    return __ctype_function(&::islower);
}
int str::isupper() {
    return __ctype_function(&::isupper);
}

str *str::strip(str *chars) {
    return lstrip(chars)->rstrip(chars);
}

str *str::lstrip(str *chars) {
    __GC_STRING remove;
    if(chars) remove = chars->unit;
    else remove = ws;
    int first = unit.find_first_not_of(remove);
    if( first == -1 ) 
        return new str("");
    return new str(unit.substr(first,unit.size()-first));
}



tuple2<str *, str *> *str::partition(str *sep)
{
    int i;
    
    i = unit.find(sep->unit);
    if(i != -1) 
        return new tuple2<str *, str *>(3, new str(unit.substr(0, i)), new str(sep->unit), new str(unit.substr(i + sep->unit.length())));
    else 
        return new tuple2<str *, str *>(3, new str(unit), new str(""), new str(""));
}

tuple2<str *, str *> *str::rpartition(str *sep)
{
    int i;
    
    i = unit.rfind(sep->unit);
    if(i != -1) 
        return new tuple2<str *, str *>(3, new str(unit.substr(0, i)), new str(sep->unit), new str(unit.substr(i + sep->unit.length())));
    else 
        return new tuple2<str *, str *>(3, new str(unit), new str(""), new str(""));
}

list<str *> *str::rsplit(str *sep, int maxsep)
{
    __GC_STRING ts;
    list<str *> *r = new list<str *>();
    int i, j, curi, tslen;
    
    curi = 0;
    i = j = unit.size() - 1;
    
    //split by whitespace
    if(!sep)
    {
        while(i > 0 && j > 0 && (curi < maxsep || maxsep < 0))
        {
            j = unit.find_last_not_of(ws, i);
            if(j == -1) break;
            
            i = unit.find_last_of(ws, j);
            
            //this works out pretty nicely; i will be -1 if no more is found, and thus i + 1 will be 0th index
            r->append(new str(unit.substr(i + 1, j - i)));
            curi++;
        }
        
        //thus we only bother about extra stuff here if we *have* found more whitespace
        if(i > 0 && j >= 0 && (j = unit.find_last_not_of(ws, i)) >= 0) r->append(new str(unit.substr(0, j)));
    }
    
    //split by seperator
    else
    {
        ts = sep->unit;
        tslen = ts.length();
        
        i++;
        while(i > 0 && j > 0 && (curi < maxsep || maxsep < 0))
        {
            j = i;
            i--;
            
            i = unit.rfind(ts, i);
            if(i == -1)
            {
                i = j;
                break;
            }
            
            r->append(new str(unit.substr(i + tslen, j - i - tslen)));
            
            curi++;
        }
        
        //either left over (beyond max) or very last match (see loop break)
        if(i >= 0) r->append(new str(unit.substr(0, i)));
    }
    
    r->reverse();
    
    return r;
}

int str::istitle(void)
{
    int i, len;
    
    len = unit.size();
    if(!len)
        return 0;

    for(i = 0; i < len; )
    {
        for( ; !::isalpha((int)unit[i]) && i < len; i++) ;
        if(i == len) break;
        
        if(!::isupper((int)unit[i])) return 0;
        i++;
        
        for( ; ::islower((int)unit[i]) && i < len; i++) ;
        if(i == len) break;
        
        if(::isalpha((int)unit[i])) return 0;
    }
    
    return 1;
}

list<str *> *str::splitlines(int keepends)
{
    list<str *> *r = new list<str *>();
    int i, j, endlen;
    const char *ends = "\r\n";
    
    endlen = i = 0;
    do
    {
        j = i + endlen;
        i = unit.find_first_of(ends, j);
        if(i == -1) break;
        
        //for all we know the character sequence could change mid-way...
        if(unit[i] == '\r' && unit[i + 1] == '\n') endlen = 2;
        else endlen = 1;
        
        r->append(new str(unit.substr(j, i - j + (keepends ? endlen : 0))));
    }
    while(i >= 0);
    
    if(j != (int)unit.size()) r->append(new str(unit.substr(j)));
    
    return r;
}



str *str::rstrip(str *chars) {
    __GC_STRING remove;
    if(chars) remove = chars->unit;
    else remove = ws;
    int last = unit.find_last_not_of(remove);
    if( last == -1 ) 
        return new str("");
    return new str(unit.substr(0,last+1));
}

list<str *> *str::split(str *sp, int max_splits) { 
    __GC_STRING s = unit;
    int sep_iter, chunk_iter = 0, tmp, num_splits = 0;
    list<str *> *result = new list<str *>();

    if (sp == NULL)
    {
#define next_separator(iter) (s.find_first_of(ws, (iter)))
#define skip_separator(iter) (s.find_first_not_of(ws, (iter)))

        if(next_separator(chunk_iter) == 0) 
            chunk_iter = skip_separator(chunk_iter);
        while((max_splits < 0 or num_splits < max_splits)
              and (sep_iter = next_separator(chunk_iter)) != -1)
        {
            result->append(new str(s.substr(chunk_iter, sep_iter - chunk_iter)));
            if((tmp = skip_separator(sep_iter)) == -1) {
                chunk_iter = sep_iter;
                break;
            } else
                chunk_iter = tmp;
            ++num_splits;
        }
        if(not (max_splits < 0 or num_splits < max_splits))
            result->append(new str(s.substr(chunk_iter, s.size()-chunk_iter))); 
        else if(sep_iter == -1)
            result->append(new str(s.substr(chunk_iter, s.size()-chunk_iter))); 

#undef next_separator
#undef skip_separator

    } else { /* given separator (slightly different algorithm required)
              * (python is very inconsistent in this respect) */
        const char *sep = sp->unit.c_str();
        int sep_size = sp->unit.size();

#define next_separator(iter) s.find(sep, (iter))
#define skip_separator(iter) ((iter + sep_size) > (int)s.size()? -1 : (iter + sep_size))

        if (max_splits == 0) {
            result->append(this);
            return result;
        }
        if(next_separator(chunk_iter) == 0) {
            chunk_iter = skip_separator(chunk_iter);
            result->append(new str());
            ++num_splits;
        }
        while((max_splits < 0 or num_splits < max_splits)
              and (sep_iter = next_separator(chunk_iter)) != -1)
        {
            result->append(new str(s.substr(chunk_iter, sep_iter - chunk_iter)));
            if((tmp = skip_separator(sep_iter)) == -1) {
                chunk_iter = sep_iter;
                break;
            } else
                chunk_iter = tmp;
            ++num_splits;
        }
        if(not (max_splits < 0 or num_splits < max_splits))
            result->append(new str(s.substr(chunk_iter, s.size()-chunk_iter))); 
        else if(sep_iter == -1)
            result->append(new str(s.substr(chunk_iter, s.size()-chunk_iter))); 


#undef next_separator
#undef skip_separator

    }

    return result;
}

str *str::translate(str *table, str *delchars) {
    if(len(table) != 256)
        throw new ValueError(new str("translation table must be 256 characters long"));

    str *newstr = new str();

    int self_size = unit.size();
    for(int i = 0; i < self_size; i++) {
        char c = unit[i];
        if(!delchars || ((int)delchars->unit.find(c)) == -1)
            newstr->unit.push_back(table->unit[(unsigned char)c]);
    }

    return newstr;
}

str *str::swapcase() {
    str *r = new str(unit);
    int len = __len__();

    for(int i = 0; i < len; i++) {
        char c = unit[i];
        if( c >= 'a' && c <= 'z' )
            r->unit[i] = ::toupper(c);
        else if( c >= 'A' && c <= 'Z' )
            r->unit[i] = ::tolower(c);
    }

    return r;
}

str *str::center(int w, str *fill) {
    int len = __len__();
    if(w<=len)
        return this;

    if(!fill) fill = sp;
    str *r = fill->__mul__(w);

    int j = (w-len)/2;
    for(int i=0; i<len; i++)
        r->unit[j+i] = unit[i];

    return r;
}

int str::__cmp__(pyobj *p) {
    str *b = (str *)p;
    int r = unit.compare(b->unit);
    if( r < 0 ) return -1;
    else if( r > 0 ) return 1;
    return 0;
}

int str::__eq__(pyobj *p) {
    return unit == ((str *)p)->unit;
}

str *str::__mul__(int n) { /* optimize */
    str *r = new str();
    if(n<=0) return r;
    __GC_STRING &s = r->unit;
    int ulen = unit.size();

    if(ulen == 1)
       r->unit = __GC_STRING(n, unit[0]);
    else {
        s.resize(ulen*n);

        for(int i=0; i<ulen*n; i+=ulen)
            s.replace(i, ulen, unit);
    }

    return r;
}
str *str::__imul__(int n) { 
    return __mul__(n);
}

/* ======================================================================== */

/* (C) 2004, 2005 Paul Hsieh. Covered under the Paul Hsieh derivative license.
   http://www.azillionmonkeys.com/qed/{hash,weblicense}.html  */

#define get16bits(d) (*((const uint16_t *) (d)))

static inline uint32_t SuperFastHash (const char * data, int len) {
    uint32_t hash = 0, tmp;
    int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= data[sizeof (uint16_t)] << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += *data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

/* ======================================================================== */

int str::__hash__() {
    if(cached_hash) 
        return cached_hash;
    cached_hash = SuperFastHash(unit.c_str(), unit.size());
    return cached_hash;

    //return __gnu_cxx::hash<char *>()(unit.c_str());
}

str *str::__add__(str *b) {
    str *s = new str();

    s->unit.reserve(unit.size()+b->unit.size());
    s->unit.append(unit);
    s->unit.append(b->unit);

    return s;
}
str *str::__iadd__(str *b) {
    return __add__(b);
}

str *__add_strs(int n, ...) {
    va_list ap;
    va_start(ap, n);

    int size;
    str *result = new str();

    size = 0;
    for(int i=0; i<n; i++) {
        size += len(va_arg(ap, str *));
    }
    va_end(ap);

    result->unit.resize(size);

    va_start(ap, n);
    size = 0;
    int pos = 0;
    for(int i=0; i<n; i++) {
        str *s = va_arg(ap, str *);

        memcpy((void *)(result->unit.data()+pos), s->unit.data(), s->unit.size());
        pos += s->unit.size();
    }
    va_end(ap);

    return result;
}

str *str::__join(pyseq<str *> *l, int total_len) {
    __GC_STRING s;
    s.resize(total_len);

    int k = 0;

    for(int i = 0; i < len(l); i++) {
        __GC_STRING &t = l->units[i]->unit;

        memcpy((void *)(s.data()+k), t.data(), t.size());
        k += t.size();

        if(unit.size()) {
            memcpy((void *)(s.data()+k), unit.data(), unit.size());
            k += unit.size();
        } 
    }

    s.resize(s.size()-unit.size());
    return new str(s);
}

str *str::join(pyiter<str *> *l) { 
    list<str *> *rl = new list<str *>();
    str *i;
    int count, total_len;
    count = total_len = 0;
    __iter<str *> *__0;
    FOR_IN(i, l, 0)
        rl->append(i);
        total_len += i->unit.size();
        ++count;
    END_FOR
    total_len += count*unit.size();

    return __join(rl, total_len);
} 

str *str::join(pyseq<str *> *l) {
    int total_len = 0;
    __GC_VECTOR(str *)::const_iterator it;
    for(it = l->units.begin(); it < l->units.end(); it++)
        total_len += (*it)->unit.size();
    total_len += len(l)*unit.size();

    return __join(l, total_len);
}

str *str::__slice__(int x, int l, int u, int s) {
    __GC_STRING r;
    slicenr(x, l, u, s, __len__());

    if(s > 0)
        for(int i=l; i<u; i += s)
            r += unit[i];
    else
        for(int i=l; i>u; i += s)
            r += unit[i];

    return new str(r);
}

int str::__fixstart(int a, int b) {
    if(a == -1) return a;
    return a+b;
}

int str::find(str *s, int a) { return __fixstart(unit.substr(a, unit.size()-a).find(s->unit), a); }
int str::find(str *s, int a, int b) { return __fixstart(unit.substr(a, b-a).find(s->unit), a); }

int str::rfind(str *s, int a) { return __fixstart(unit.substr(a, unit.size()-a).rfind(s->unit), a); }
int str::rfind(str *s, int a, int b) { return __fixstart(unit.substr(a, b-a).rfind(s->unit), a); }

int str::__checkneg(int i) {
    if(i == -1)
        throw new ValueError(new str("substring not found"));
    return i;
}

int str::index(str *s, int a) { return __checkneg(find(s, a)); }
int str::index(str *s, int a, int b) { return __checkneg(find(s, a, b)); }

int str::rindex(str *s, int a) { return __checkneg(find(s, a)); }
int str::rindex(str *s, int a, int b) { return __checkneg(find(s, a, b)); }

int str::count(str *s, int start) { return count(s, start, __len__()); }
int str::count(str *s, int start, int end) {
    int i, count, one = 1;
    slicenr(7, start, end, one, __len__());

    i = start; count = 0;
    while( ((i = unit.find(s->unit, i)) != -1) && (i <= end-len(s)) )
    {
        i += len(s);
        count++;
    }

    return count; 
}

int str::startswith(str *s, int start) { return startswith(s, start, __len__()); }
int str::startswith(str *s, int start, int end) {
    int i, j, one = 1;
    slicenr(7, start, end, one, __len__());

    for(i = start, j = 0; i < end && j < len(s); )
        if (unit[i++] != s->unit[j++])
            return 0;

    return j == len(s); 
}

int str::endswith(str *s, int start) { return endswith(s, start, __len__()); }
int str::endswith(str *s, int start, int end) {
    int i, j, one = 1;
    slicenr(7, start, end, one, __len__());

    for(i = end, j = len(s); i > start && j > 0; )
        if (unit[--i] != s->unit[--j])
            return 0; 

    return 1;
}

str *str::replace(str *a, str *b, int c) {
    __GC_STRING s = unit;
    int i, j = 0;
    while( ((c==-1) || (j++ != c)) && (i = s.find(a->unit)) != -1 ) 
        s.replace(i, a->unit.size(), b->unit);
    return new str(s);
}

str *str::upper() {
    if(unit.size() == 1)
        return __char_cache[::toupper(unit[0])];

    str *toReturn = new str(*this);
    std::transform(toReturn->unit.begin(), toReturn->unit.end(), toReturn->unit.begin(), toupper);

    return toReturn;
}

str *str::lower() {
    if(unit.size() == 1)
        return __char_cache[::tolower(unit[0])];

    str *toReturn = new str(*this);
    std::transform(toReturn->unit.begin(), toReturn->unit.end(), toReturn->unit.begin(), tolower);

    return toReturn;
}

str *str::title() {
    str *r = new str(unit);
    int i = 0;
    while( (i != -1) && (i<(int)unit.size()) )
    {
        r->unit[i] = ::toupper(r->unit[i]);
        i = unit.find(" ", i);
        if (i != -1)
            i++;
    }
    return r;
}

str *str::capitalize() {
    str *r = new str(unit);
    r->unit[0] = ::toupper(r->unit[0]);
    return r;
}

/* str *str::sorted() {
    str *s = new str(unit);
    sort(s->unit.begin(), s->unit.end());
    return s;
} */

#ifdef __SS_BIND
str::str(PyObject *p) : cached_hash(0) {
    if(!PyString_Check(p)) 
        throw new TypeError(new str("error in conversion to Shed Skin (string expected)"));

    __class__ = cl_str_;
    unit = __GC_STRING(PyString_AsString(p), PyString_Size(p));
}

PyObject *str::__to_py__() {
    return PyString_FromStringAndSize(unit.c_str(), unit.size());
}
#endif

class_::class_(const char *name, int low, int high) {
    this->__name__ = new str(name);
    this->low = low; this->high = high;
}

str *class_::__repr__() {
    return (new str("class "))->__add__(__name__);
}

int class_::__eq__(pyobj *c) {
    return c == this;
}

/* file methods */

file::file() {
    endoffile=print_space=0;
    print_lastchar='\n';
}

file::file(FILE *g) {
    f = g;
    endoffile=print_space=0;
    print_lastchar='\n';
}

file::file(str *name, str *flags) {
    if (!flags)
        flags = new str("r");
    f = fopen(name->unit.c_str(), flags->unit.c_str());
    this->name = name;
    this->mode = flags;
    if(!f) 
       throw new IOError(__mod(new str("No such file or directory: '%s'"), name));
    endoffile=print_space=0;
    print_lastchar='\n';
}

file *open(str *name, str *flags) {
    return new file(name, flags);
}

int file::getchar() {
    __check_closed();
    return fgetc(f);
}

int file::putchar(int c) {
    __check_closed();
    fputc(c, f);
    return 0;
}

int file::write(str *s) {
    __check_closed();
  //  fputs(s->unit.c_str(), f);

    for(int i = 0; i < (int)s->unit.size(); i++)
        putchar(s->unit[i]);

    return 0;
}

void file::__check_closed() {
    if(closed)
        throw new ValueError(new str("I/O operation on closed file"));
}

int file::seek(int i, int w) {
    __check_closed();
    fseek(f, i, w);
    endoffile = 0; /* XXX add check */
    return 0;
}

int file::tell() {
    __check_closed();
    return ftell(f);
}

int file::writelines(pyseq<str *> *l) {
    __check_closed();
    for(int i=0; i<len(l); i++)
        write(l->__getitem__(i));
    return 0;
}

str *file::readline(int n) { 
    __check_closed();
    int i = 0;
    str *r = new str();

    while((n==-1) || (i < n)) {
        int c = getchar();
        if(c == EOF) {
            endoffile = 1;
            break;
        }
        r->unit += c;
        if(c == '\n')
            break;
        i += 1;
    }

    return r;
}

str *file::read(int n) {
    __check_closed();
    int i = 0;
    str *r = new str();

    while((n==-1) || (i < n)) {
        int c = getchar();
        if(c == EOF) {
            endoffile = 1;
            break;
        }
        r->unit += c;
        i += 1;
    }

    return r;
}

list<str *> *file::readlines() {
    __check_closed();
    list<str *> *lines = new list<str *>();
    while(!endoffile) {
        str *line = readline(); 
        if(endoffile && !len(line))
            break;
        lines->append(line);
    } 

    return lines;
}

int file::flush() {
    __check_closed();
    fflush(f);
    return 0;
}

int file::close() {
    fclose(f);
    closed = 1;
    return 0;
}

int file::__ss_fileno() {
    __check_closed();
    return fileno(f);
}

str *file::__repr__() {
    return (new str("file '"))->__add__(name)->__add__(new str("'"));
}

/* builtin functions */

str *pyobj::__repr__() {
    return __class__->__name__->__add__(new str(" instance"));
}

int whatsit(__GC_STRING &s) {
    int i = -1;
    int count = 0;

    while((i = s.find("%", i+1)) > -1)
    {
        int j = s.find_first_of("diouxXeEfFgGhcrs", i);
        s.replace(i, j-i+1, "%s");
        count += 1;
    }

    return count;
}

str *raw_input(str *msg) {
    __GC_STRING s;
    if(msg)
        std::cout << msg->unit;
    std::getline(std::cin, s);
    return new str(s); 
}

int __int() { return 0; }

template<> int __int(str *s) { return __int(s, 10); }
template<> int __int(int i) { return i; }
template<> int __int(bool b) { return b; }
template<> int __int(double d) { return (int)d; }

int __int(str *s, int base) {
    char *cp;
    s = s->strip();
    int i = strtol(s->unit.c_str(), &cp, base);
    if(cp != s->unit.c_str()+s->unit.size())
        throw new ValueError(new str("invalid literal for int()"));
    return i;
}

double __float() { return 0; }
template<> double __float(int p) { return p; }
template<> double __float(bool b) { return __float((int)b); }
template<> double __float(double d) { return d; }
template<> double __float(str *s) {
    return atof((char *)(s->unit.c_str()));
}

int isinstance(pyobj *p, class_ *c) {
    int classnr = p->__class__->low;
    return ((classnr >= c->low) && (classnr <= c->high));
}

int isinstance(pyobj *p, tuple2<class_ *, class_ *> *t) {
    int classnr = p->__class__->low;
    for(int i = 0; i < t->__len__(); i++)
    {
       class_ *c = t->__getitem__(i);
       if ((classnr >= c->low) && (classnr <= c->high))
           return 1;
    }
    return 0;
}

static int range_len(int lo, int hi, int step) {
    /* modified from CPython */
    int n = 0;
    if ((lo < hi) && (step>0)) {
        unsigned int uhi = (unsigned int)hi;
        unsigned int ulo = (unsigned int)lo;
        unsigned int diff = uhi - ulo - 1;
        n = (int)(diff / (unsigned int)step + 1);
    }
    else {
        if ((lo > hi) && (step<0)) {
            unsigned int uhi = (unsigned int)lo;
            unsigned int ulo = (unsigned int)hi;
            unsigned int diff = uhi - ulo - 1;
            n = (int)(diff / (unsigned int)(-step) + 1);
        }
    }
    return n;
}

list<int> *range(int a, int b, int s) {
    list<int> *r;
    int i, pos;

    r = new list<int>();
    pos = 0;
    i = a;

    if(s==0)
        throw new ValueError(new str("range() step argument must not be zero"));

    if(s==1) {
        r->units.resize(b-a);
        for(; i<b;i++)
            r->units[pos++] = i;

        return r;
    }

    r->units.resize(range_len(a,b,s));

    if(s>0) {
        while((i<b)) {
            r->units[pos++] = i;
            i += s;
        }
    }
    else {
        while((i>b)) {
            r->units[pos++] = i;
            i += s;
        }
    }

    return r;
}

list<int> *range(int n) {
    return range(0, n);
}

class __rangeiter : public __iter<int> {
public:
    int i, a, b, s;

    __rangeiter(int a, int b, int s) {
        this->__class__ = cl_rangeiter;

        this->a = a;
        this->b = b;
        this->s = s;
        i = a;
        if(s==0)
            throw new ValueError(new str("xrange() arg 3 must not be zero"));
    }

    int next() {
        if(s>0) {
            if(i<b) {
                i += s;
                return i-s;
            }
        }
        else if(i>b) {
                i += s;
                return i-s;
        }

        throw new StopIteration();
    }

    int __len__() {
        return range_len(a,b,s);
    }
};

__iter<int> *xrange(int a, int b, int s) { return new __rangeiter(a,b,s); }
__iter<int> *xrange(int n) { return new __rangeiter(0, n, 1); }

__iter<int> *reversed(__rangeiter *x) {
   return new __rangeiter(x->a+(range_len(x->a,x->b,x->s)-1)*x->s, x->a-x->s, -x->s);
}

int ord(str *s) {
    return (unsigned char)(s->unit[0]);
}

str *chr(int i) {
    return __char_cache[i];
}

/* copy, deepcopy */

template<> int __deepcopy(int i, dict<void *, pyobj *> *) { return i; }
template<> double __deepcopy(double d, dict<void *, pyobj *> *) { return d; }
template<> void *__deepcopy(void *d, dict<void *, pyobj *> *) { return d; }

template<> int __copy(int i) { return i; }
template<> double __copy(double d) { return d; }
template<> void *__copy(void *d) { return d; }

/* representation */

template<> str *repr(double d) { return __str(d); }
template<> str *repr(int i) { return __str(i); }
template<> str *repr(bool b) { return __str((int)b); }
template<> str *repr(void *v) { return new str("void"); }

str *__str(void *v) { return new str("void"); }

/* equality, comparison, math operators */

template<> int __eq(int a, int b) { return a == b; }
template<> int __eq(double a, double b) { return a == b; }
template<> int __eq(void *a, void *b) { return a == b; }
template<> int __ne(int a, int b) { return a != b; }
template<> int __ne(double a, double b) { return a != b; }
template<> int __ne(void *a, void *b) { return a != b; }
template<> int __gt(int a, int b) { return a > b; }
template<> int __gt(double a, double b) { return a > b; }
template<> int __ge(int a, int b) { return a >= b; }
template<> int __ge(double a, double b) { return a >= b; }
template<> int __lt(int a, int b) { return a < b; }
template<> int __lt(double a, double b) { return a < b; }
template<> int __le(int a, int b) { return a <= b; }
template<> int __le(double a, double b) { return a <= b; }
template<> int __add(int a, int b) { return a + b; }
template<> double __add(double a, double b) { return a + b; }

/* get class pointer */

template<> class_ *__type(int i) { return cl_int_; }
template<> class_ *__type(double d) { return cl_float_; }

/* hashing */

template<> int hasher(int a) { return a; }
template<> int hasher(double v) {
    int hipart, expo; /* modified from CPython */
    v = frexp(v, &expo);
    v *= 32768.0; /* 2**15 */
    hipart = (int)v;   /* take the top 16 bits */
    v = (v - (double)hipart) * 32768.0; /* get the next 16 bits */
    return hipart + (int)v + (expo << 15);
}
template<> int hasher(void *a) { return (intptr_t)a; }

/* pow */

template<> double __power(double a, double b) { return pow(a,b); }
template<> double __power(int a, double b) { return pow(a,b); }
template<> double __power(double a, int b) { return pow(a,b); }

template<> int __power(int a, int b) {
    int res, tmp;

    res = 1;
    tmp = a;

    while((b>0)) {
        if ((b%2)) {
            res = (res*tmp);
        }
        tmp = (tmp*tmp);
        b = (b/2);
    }
    return res;
}

int __power(int a, int b, int c) {
    int res, tmp;

    res = 1;
    tmp = a;

    while((b>0)) {
        if ((b%2)) {
            res = ((res*tmp)%c);
        }
        tmp = ((tmp*tmp)%c);
        b = (b/2);
    }
    return res;
}

int __power2(int a) { return a*a; }
double __power2(double a) { return a*a; }
int __power3(int a) { return a*a*a; }
double __power3(double a) { return a*a*a; }

/* division */

template<> double __divs(double a, double b) { return a/b; }
template<> double __divs(int a, double b) { return (double)a/b; }
template<> double __divs(double a, int b) { return a/((double)b); }
template<> int __divs(int a, int b) { return (int)floor(((double)a)/b); }

template<> double __floordiv(double a, double b) { return floor(a/b); }
template<> double __floordiv(int a, double b) { return floor((double)a/b); }
template<> double __floordiv(double a, int b) { return floor(a/((double)b)); }
template<> int __floordiv(int a, int b) { return (int)floor((double)a/b); }

template<> tuple2<double, double> *divmod(double a, double b) {
    return new tuple2<double, double>(2, __floordiv(a,b), __mods(a,b));
}
template<> tuple2<double, double> *divmod(double a, int b) { return divmod(a, (double)b); } 
template<> tuple2<double, double> *divmod(int a, double b) { return divmod((double)a, b); }
template<> tuple2<int, int> *divmod(int a, int b) {
    return new tuple2<int, int>(2, __floordiv(a,b), __mods(a,b));
}

/* slicing */

void slicenr(int x, int &l, int&u, int&s, int len) {
    if((x&4) && (s == 0))
        throw new ValueError(new str("slice step cannot be zero"));

    if (!(x&4))
        s = 1;

    if (l<0)
        l = len+l;
    if (u<0)
        u = len+u;

    if (l<0)
        l = 0;
    if (u>=len)
        u = len;

    if(s<0) {
        if (!(x&1))
            l = len-1;
        if (!(x&2))
            u = -1;
    }
    else {
        if (!(x&1))
            l = 0;
        if (!(x&2))
            u = len;
    }
}

/* cmp */

template<> int __cmp(int a, int b) { 
    if(a < b) return -1;
    else if(a > b) return 1;
    return 0;
} 

template<> int __cmp(double a, double b) {
    if(a < b) return -1;
    else if(a > b) return 1;
    return 0;
}
template<> int __cmp(void *a, void *b) {
    if(a < b) return -1;
    else if(a > b) return 1;
    return 0;
}

str *__str(int i, int base) {
    if(i<10 && i>=0 && base==10)
        return __char_cache['0'+i];

    char asc[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char buf[12];
    char *psz = buf+11;
    if(i==INT_MIN)
        return new str("-2147483648");
    int neg = i<0;
    *psz = 0;
    if(neg) i = -i;
    do {
        unsigned lsd = i%base;
        i = i/base;
        *(--psz) = asc[lsd];
    }
    while(i != 0);
    if(neg) *(--psz) = '-';
    return new str(psz);
}

str *__str(bool b) {
    return __str((int)b);
}

template<> str *hex(int i) {
    return (new str("0x"))->__add__(__str(i, 16));
}
template<> str *hex(bool b) { return hex((int)b); }

template<> str *oct(int i) {
    if(i==0) return new str("0");
    return (new str("0"))->__add__(__str(i, 8));
}
template<> str *oct(bool b) { return oct((int)b); }

str *__str() { return new str(""); } /* XXX optimize */

template<> str *__str(double t) {
    std::stringstream ss;
    ss.precision(12);
    ss << std::showpoint << t;
    __GC_STRING s = ss.str().c_str();
    if( ((int)s.find('e')) == -1)
    {
        int j = s.find_last_not_of("0");
        if( s[j] == '.') j++;
        s = s.substr(0, j+1);
    }

    return new str(s);
}

double ___round(double a) {
    return __portableround(a);
}
double ___round(double a, int n) {
    return __portableround(pow(10,n)*a)/pow(10,n);
}

/* bool */

int __bool() { return 0; }

template<> int __bool(int x) {
    return x;
}
template<> int __bool(bool x) {
    return (int)x;
}
template<> int __bool(double x) {
    return x!=0;
}

/* sum */

int __sum(pyseq<int> *l, int b) { return accumulate(l->units.begin(), l->units.end(), b); }
double __sum(pyseq<double> *l, double b) { return accumulate(l->units.begin(), l->units.end(), b); }

/* min, max */

int __min(pyseq<int> *l) { return __minimum(l); }
double __min(pyseq<double> *l) { return __minimum(l); }
int __max(pyseq<int> *l) { return __maximum(l); }
double __max(pyseq<double> *l) { return __maximum(l); }

#define __ss_max(a,b) ((a) > (b) ? (a) : (b))
#define __ss_max3(a,b,c) (__ss_max((a), __ss_max((b), (c))))

template<> int __max(int a, int b) { return __ss_max(a,b); }
template<> int __max(int a, int b, int c) { return __ss_max3(a,b,c); }
template<> double __max(double a, double b) { return __ss_max(a,b); }
template<> double __max(double a, double b, double c) { return __ss_max3(a,b,c); }

#define __ss_min(a,b) ((a) < (b) ? (a) : (b))
#define __ss_min3(a,b,c) (__ss_min((a), __ss_min((b), (c))))

template<> int __min(int a, int b) { return __ss_min(a,b); }
template<> int __min(int a, int b, int c) { return __ss_min3(a,b,c); }
template<> double __min(double a, double b) { return __ss_min(a,b); }
template<> double __min(double a, double b, double c) { return __ss_min3(a,b,c); }

/* abs */

template<> int __abs(int a) { return a<0?-a:a; }
template<> double __abs(double a) { return a<0?-a:a; }
int __abs(bool b) { return __abs((int)b); }

/* list */

list<str *> *__list(str *s) {
    list<str *> *r = new list<str *>();
    r->units.resize(len(s));
    int sz = s->unit.size();
    for(int i=0; i<sz; i++) 
        r->units[i] = __char_cache[s->unit[i]];
    return r;
}

/* sorted */

list<str *> *sorted(str *t, int (*cmp)(str *, str *), int key, int reverse) {
    list<str *> *l = __list(t);
    l->sort(cmp, key, reverse);
    return l;
}
list<str *> *sorted(str *t, int cmp, int key, int reverse) {
    return sorted(t, (int (*)(str *, str *))0, key, reverse);
}

/* mod helpers */

#if defined(WIN32) || defined(__sun)
int vasprintf(char **ret, const char *format, va_list ap)
{
    va_list ap2;
    int len= 100;        /* First guess at the size */
    if ((*ret= (char *)malloc(len)) == NULL) return -1;
    while (1)
    {
        int nchar;
        va_copy(ap2, ap);
        nchar= vsnprintf(*ret, len, format, ap2);
        if (nchar > -1 && nchar < len) return nchar;
        if (nchar > len)
            len= nchar+1;
        else
            len*= 2;
        if ((*ret= (char *)realloc(*ret, len)) == NULL)
        {
            free(*ret);
            return -1;
        }
    }
}

int asprintf(char **ret, const char *format, ...)
{
    va_list ap;
    int nc;
    va_start (ap, format);
    nc= vasprintf(ret, format, ap);
    va_end(ap);
    return nc;
}
#endif

int __fmtpos(str *fmt) {
    int i = fmt->unit.find('%');
    if(i == -1)
        return -1;
    return fmt->unit.find_first_of(__fmtchars, i+1);
}

void __fmtcheck(str *fmt, int l) {
    int i = 0, j = 0;
    while((j = fmt->unit.find('%', j)) != -1) {
        char c = fmt->unit[j+1];
        if(c != '%')
            i++;    
        j += 2;
    }

    if(i < l)
        throw new TypeError(new str("not all arguments converted during string formatting"));
    if(i > l)
        throw new TypeError(new str("not enough arguments for format string"));

}

str *__mod2(str *fmt, va_list args) {
    int j;
    str *r = new str();

    while((j = __fmtpos(fmt)) != -1) {
        char c = fmt->unit[j];

        if(c == 'c')
            __modfill(&fmt, va_arg(args, str *), &r);
        else if(c == 's' || c == 'r')
            __modfill(&fmt, va_arg(args, pyobj *), &r);
        else if(c == '%')
            __modfill(&fmt, 0, &r);
        else if(((int)__GC_STRING("diouxX").find(c)) != -1)
            __modfill(&fmt, va_arg(args, int), &r);
        else if(((int)__GC_STRING("eEfFgGh").find(c)) != -1)
            __modfill(&fmt, va_arg(args, double), &r);
        else
            break;

    }

    r->unit += fmt->unit;
    return r;
}

/* mod */

str *mod_to_c(str *s) { return s; } 
str *mod_to_c(int i) { return chr(i); }
str *mod_to_c(double d) { return chr(__int(d)); }

str *__mod(str *fmt, ...) {
     va_list args;
     va_start(args, fmt);
     str *s = __mod2(new str(fmt->unit), args);
     va_end(args);
     return s;
}

/* print .., */

char print_lastchar = '\n';
int print_space = 0;

void __exit() {
    if(print_lastchar != '\n')
        std::cout << '\n';
}

void print(const char *fmt, ...) { // XXX merge four functions (put std::cout in a file instance)
     va_list args;
     va_start(args, fmt);
     str *s = __mod2(new str(fmt), args);

     if(print_space && print_lastchar != '\n' && !(len(s) && s->unit[0] == '\n'))
         std::cout << " ";

     std::cout << s->unit;
     va_end(args);

     print_lastchar = s->unit[len(s)-1];
     print_space = 0;
}

void print(file *f, const char *fmt, ...) {
     va_list args;
     va_start(args, fmt);
     str *s = __mod2(new str(fmt), args);

     if(f->print_space && f->print_lastchar != '\n' && !(len(s) && s->unit[0] == '\n'))
         f->putchar(' ');

     f->write(s);
     va_end(args);

     f->print_lastchar = s->unit[len(s)-1];
     f->print_space = 0;
}

void printc(const char *fmt, ...) {
     va_list args;
     va_start(args, fmt);
     str *s = __mod2(new str(fmt), args);

     if(print_space && print_lastchar != '\n' && !(len(s) && s->unit[0] == '\n'))
         std::cout << " ";

     std::cout << s->unit;
     va_end(args);

     if(len(s)) print_lastchar = s->unit[len(s)-1];
     else print_lastchar = ' ';
     print_space = 1;
}

void printc(file *f, const char *fmt, ...) {
     va_list args;
     va_start(args, fmt);
     str *s = __mod2(new str(fmt), args);

     if(f->print_space && f->print_lastchar != '\n' && !(len(s) && s->unit[0] == '\n'))
         f->putchar(' ');

     f->write(s);
     va_end(args);

     if(len(s)) f->print_lastchar = s->unit[len(s)-1];
     else f->print_lastchar = ' ';
     f->print_space = 1;
}

/* str, file iteration */

__seqiter<str *> *str::__iter__() {
    return new __striter(this);
}

__striter::__striter(str *p) {
    this->p = p;
    counter = 0;
}

str *__striter::next() {
    if(counter == (int)p->unit.size())
        throw new StopIteration();
    return p->__getitem__(counter++); 
}

__iter<str *> *file::__iter__() {
    return new __fileiter(this);
}

__fileiter::__fileiter(file *p) {
    this->p = p;
}

str *__fileiter::next() {
    if(p->endoffile)
        throw new StopIteration();
    str *line = p->readline();
    if(p->endoffile && !len(line))
        throw new StopIteration();
    return line;
}

/* mod */

template<> double __mods(double a, double b) {
    double f = fmod(a,b);
    if((f<0 && b>0)||(f>0 && b<0)) f+=b;
    return f;
}
template<> double __mods(int a, double b) { return __mods((double)a, b); }
template<> double __mods(double a, int b) { return __mods(a, (double)b); }

template<> int __mods(int a, int b) {
    int m = a%b;
    if((m<0 && b>0)||(m>0 && b<0)) m+=b;
    return m;
}

/* binding */

#ifdef __SS_BIND
PyObject *__import(char *mod, char *method) {
    PyObject *m = PyImport_ImportModule(mod);
    PyObject *d = PyObject_GetAttrString(m, (char *)"__dict__");
    return PyDict_GetItemString(d, method);
}

PyObject *__call(PyObject *obj, PyObject *args) {
    return PyObject_CallObject(obj, args);
}

PyObject *__call(PyObject *obj, char *name, PyObject *args) {
    PyObject *method = PyObject_GetAttrString(obj, name);
    PyObject *x = PyObject_CallObject(method, args);
    return x;
}

PyObject *__args(int n, ...) {
    va_list ap;
    va_start(ap, n);

    PyObject *p = PyTuple_New(n);

    for(int i=0; i<n; i++) {
        PyObject *t = va_arg(ap, PyObject *);
        PyTuple_SetItem(p, i, t);
    }
    va_end(ap);
    return p;
}

template<> PyObject *__to_py(int i) { return PyInt_FromLong(i); }   
template<> PyObject *__to_py(double d) { return PyFloat_FromDouble(d); }

template<> int __to_ss(PyObject *p) { 
    if(p==Py_None) return 0;
    if(!PyInt_Check(p)) 
        throw new TypeError(new str("error in conversion to Shed Skin (integer expected)"));
    return PyInt_AsLong(p);
}

template<> double __to_ss(PyObject *p) { 
    if(p==Py_None) return 0.0;
    if(!PyInt_Check(p) and !PyFloat_Check(p)) 
        throw new TypeError(new str("error in conversion to Shed Skin (float or int expected)"));
    return PyFloat_AsDouble(p); 
}

#endif

// Exceptions
OSError::OSError(str *filename) {
    this->filename = filename;
    __ss_errno = errno;
    message = new str("");
    strerror = new str(::strerror(__ss_errno));
}
str *OSError::__str__() {
    return __add_strs(7, new str("[Errno "), __str(__ss_errno), new str("] "), strerror, new str(": '"), filename, new str("'"));
}
str *OSError::__repr__() {
    return __add_strs(5, new str("OSError("), __str(__ss_errno), new str(", '"), strerror, new str("')")); 
}

} // namespace __shedskin__


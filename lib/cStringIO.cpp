#include <stdio.h>
#include "cStringIO.hpp"

namespace __cStringIO__ {

StringI::StringI(str *s) : file() {
    pos = 0;
    if(s) this->s = s;
    else this->s = new str();

}

void *StringI::seek(int i, int w) {
    if(w==0) pos = i;
    else if(w==1) pos += i;
    else pos = len(s)+i;
    endoffile = 0;
    return NULL;
}

int StringI::getchar() {
    if(pos == len(s))
        return EOF;
    return s->unit[pos++];
}

void *StringI::putchar(int c) {
    if(pos < len(s))
        s->unit[pos] = c;
    else 
        s->unit += c;

    pos++;
    return NULL;
}

StringI *StringIO(str *s) {
    return (new StringI(s));

}

void __init() {

}

} // module namespace


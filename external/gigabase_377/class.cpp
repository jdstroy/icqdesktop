//-< CLASS.CPP >-----------------------------------------------------*--------*
// GigaBASE                  Version 1.0         (c) 1999  GARRET    *     ?  *
// (Post Relational Database Management System)                      *   /\|  *
//                                                                   *  /  \  *
//                          Created:     20-Nov-98    K.A. Knizhnik  * / [] \ *
//                          Last update:  1-Jan-99    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Metaclass information
//-------------------------------------------------------------------*--------*

#define INSIDE_GIGABASE

#include "gigabase.h"
#include "compiler.h"
#include "symtab.h"

BEGIN_GIGABASE_NAMESPACE

#ifndef CHECK_RECORD_SIZE
#define CHECK_RECORD_SIZE 1
#endif

const size_t MAX_MSG_SIZE = 1024;

dbTableDescriptor* dbTableDescriptor::chain;
dbMutex* dbTableDescriptor::chainMutex;

void* dbFieldDescriptor::operator new(size_t size)
{
    return dbMalloc(size);
}

void  dbFieldDescriptor::operator delete(void* p)
{
    dbFree(p);
}


dbFieldDescriptor::dbFieldDescriptor(char_t const* name)
{
    next = prev = this;
    this->name = (char_t*)name;
    longName = NULL;
    dbSymbolTable::add(this->name, tkn_ident, GB_CLONE_ANY_IDENTIFIER);
    appOffs = dbsOffs = 0;
    defTable = refTable = NULL;
    refTableName = NULL;
    inverseRef = NULL;
    components = NULL;
    indexType = 0;
    method = NULL;
    attr = OneToOneMapping;
    bTree = 0;
    hashTable = 0;
    comparator = (dbUDTComparator)&memcmp;
	defaultValue.Db_Int8 = 0;
}


dbFieldDescriptor::dbFieldDescriptor(char_t const*      fieldName,
                                     size_t             offs,
                                     size_t             size,
                                     int                index,
                                     char_t const*      inverse,
                                     dbFieldDescriptor* fieldComponents)
{
    next = prev = this;
    name = (char_t*)fieldName;
    longName = NULL;
    dbSymbolTable::add(name, tkn_ident, GB_CLONE_ANY_IDENTIFIER);
    appOffs = (int)offs;
    dbsOffs = 0;
    alignment = appSize = dbsSize = (int)size;
    defTable = refTable = NULL;
    refTableName = NULL;
    indexType = index;
    type = appType = dbField::tpStructure;
    inverseRefName = (char_t*)inverse;
    if (inverseRefName != NULL) {
        dbSymbolTable::add(inverseRefName, tkn_ident, GB_CLONE_ANY_IDENTIFIER);
    }
    inverseRef = NULL;
    components = fieldComponents;
    method = NULL;
    attr = 0;
    bTree = 0;
    hashTable = 0; 
    comparator = (dbUDTComparator)&memcmp;
	defaultValue.Db_Int8 = 0;
}

dbFieldDescriptor::~dbFieldDescriptor() 
{
    if (type == dbField::tpString) { 
        delete components;
    } else if (type == dbField::tpStructure) {
        dbFieldDescriptor* last = components->prev;;
        while (last->method != NULL) { 
            dbFieldDescriptor* prev = last->prev;
            delete last->method;
            delete last;
            if (last == components) { 
                break;
            }
            last = prev;
        }
    }
    delete[] longName; 
}

dbFieldDescriptor* dbFieldDescriptor::find(char_t const* name)
{
    char_t* symnam = (char_t*)name;
    dbSymbolTable::add(symnam, tkn_ident, GB_CLONE_ANY_IDENTIFIER);
    return findSymbol(symnam);
}

dbFieldDescriptor* dbFieldDescriptor::findSymbol(const char_t* name)
{
    dbFieldDescriptor* field = components;
    do {
        if (field->name == name) {
            return field;
        }
    } while ((field = field->next) != components);
    return NULL;
}


size_t dbFieldDescriptor::calculateRecordSize(byte* base, size_t offs)
{
    dbFieldDescriptor* fd = this;
    do {
        switch (fd->appType) { 
          case dbField::tpArray:
          { 
            int nElems = (int)((dbAnyArray*)(base + fd->appOffs))->length();
            offs = DOALIGN(offs, fd->components->alignment)
                 + nElems*fd->components->dbsSize;
            if (fd->attr & HasArrayComponents) {
                byte* elem = (byte*)((dbAnyArray*)(base+fd->appOffs))->base();
                dbFieldDescriptor* component = fd->components;
                size_t elemSize = component->appSize;
                while (--nElems >= 0) {
                    offs = component->calculateRecordSize(elem, offs);
#ifdef GIGABASE_PROTECT
					if (offs <= 0)
						return 0;
#endif
                    elem += elemSize;
                }
            }
            continue;
          }
#ifdef USE_MFC_STRING
          case dbField::tpMfcString:
            offs = DOALIGN(offs, sizeof(MFC_STRING::XCHAR));
            offs += (((MFC_STRING*)(base + fd->appOffs))->GetLength() + 1)*sizeof(MFC_STRING::XCHAR);
            continue;
#endif
#ifdef USE_STD_STRING
          case dbField::tpStdString:
            offs = DOALIGN(offs, sizeof(char_t));
            offs += (((STD_STRING*)(base + fd->appOffs))->length() + 1)*sizeof(char_t);
            continue;
#endif
          case dbField::tpString:
          {
            char_t* str = *(char_t**)(base + fd->appOffs);
            if (str == NULL) { 
                offs = DOALIGN(offs, sizeof(char_t));
                offs += sizeof(char_t);
            } else { 
                offs = DOALIGN(offs, sizeof(char_t));
#ifdef GIGABASE_PROTECT
                int iLen = DBSTRLEN(str);
				assert(iLen < GIGABASE_MAX_STRING_LENGTH);
				if (iLen >= GIGABASE_MAX_STRING_LENGTH)
					return 0;
                offs += (iLen + 1)*sizeof(char_t);
#else
                offs += (STRLEN(str) + 1)*sizeof(char_t);
#endif
            }
            continue;
          }
          default:
            if (fd->attr & HasArrayComponents) {
                offs = fd->components->calculateRecordSize(base+fd->appOffs, offs);
#ifdef GIGABASE_PROTECT
				if (offs <= 0)
					return 0;
#endif
            }
        }
    } while ((fd = fd->next) != this);
    return offs;
}


size_t dbFieldDescriptor::calculateNewRecordSize(byte* base, size_t offs)
{
    dbFieldDescriptor* fd = this;
    do {
        if (fd->type == dbField::tpArray) {
            if (fd->oldDbsType == dbField::tpUnknown) {
                continue;
            }
            int nElems = ((dbVarying*)(base + fd->oldDbsOffs))->size;
            offs = DOALIGN(offs, fd->components->alignment)
                 + nElems*fd->components->dbsSize;
            if (fd->attr & HasArrayComponents) {
                byte* elem = base + ((dbVarying*)(base+fd->oldDbsOffs))->offs;
                while (--nElems >= 0) {
                    offs = fd->components->calculateNewRecordSize(elem, offs);
                    elem += fd->components->oldDbsSize;
                }
            }
        } else if (fd->type == dbField::tpString) {
            offs = DOALIGN(offs, fd->components->alignment);
            if (fd->oldDbsType == dbField::tpUnknown) {
                offs += sizeof(char_t);
            } else {
                offs += ((dbVarying*)(base + fd->oldDbsOffs))->size*sizeof(char_t);
            }
        } else if (fd->attr & HasArrayComponents) {
            offs = fd->components->calculateNewRecordSize(base, offs);
        }
    } while ((fd = fd->next) != this);
    return offs;
}

void dbFieldDescriptor::fetchRecordFields(byte* dst, byte* src)
{
    dbFieldDescriptor* fd = this;
    do {
        switch (fd->appType) {
          case dbField::tpBool:
            *(bool*)(dst+fd->appOffs) = *(bool*)(src+fd->dbsOffs);
            break;
          case dbField::tpInt1:
            *(int1*)(dst+fd->appOffs) = *(int1*)(src+fd->dbsOffs);
            break;
          case dbField::tpInt2:
            *(int2*)(dst+fd->appOffs) = *(int2*)(src+fd->dbsOffs);
            break;
          case dbField::tpInt4:
            *(int4*)(dst+fd->appOffs) = *(int4*)(src+fd->dbsOffs);
            break;
          case dbField::tpInt8:
            *(db_int8*)(dst+fd->appOffs) = *(db_int8*)(src+fd->dbsOffs);
            break;
          case dbField::tpReal4:
            *(real4*)(dst+fd->appOffs) = *(real4*)(src+fd->dbsOffs);
            break;
          case dbField::tpReal8:
            *(real8*)(dst+fd->appOffs) = *(real8*)(src+fd->dbsOffs);
            break;
          case dbField::tpRawBinary:
            memcpy(dst+fd->appOffs, src+fd->dbsOffs, fd->dbsSize);
            break;
#ifdef USE_MFC_STRING
          case dbField::tpMfcString:
            *(MFC_STRING*)(dst + fd->appOffs) = (MFC_STRING::PCXSTR)(src + ((dbVarying*)(src+fd->dbsOffs))->offs); 
            break;
#endif
#ifdef USE_STD_STRING
          case dbField::tpStdString:
            ((STD_STRING*)(dst + fd->appOffs))->assign((char_t*)(src + ((dbVarying*)(src+fd->dbsOffs))->offs), 
                                                      ((dbVarying*)(src+fd->dbsOffs))->size - 1);                
            break;
#endif
          case dbField::tpString:
            *(char_t**)(dst + fd->appOffs) =
                (char_t*)(src + ((dbVarying*)(src+fd->dbsOffs))->offs);
            break;
          case dbField::tpArray:
            {
                int nElems = ((dbVarying*)(src+fd->dbsOffs))->size;
                byte* srcElem = src + ((dbVarying*)(src+fd->dbsOffs))->offs;
                dbAnyArray* array = (dbAnyArray*)(dst+fd->appOffs);
                if (fd->attr & dbFieldDescriptor::OneToOneMapping) {
                    fd->arrayAllocator(array, srcElem, nElems);
                } else {
                    fd->arrayAllocator(array, NULL, nElems);
                    byte* dstElem = (byte*)array->base();
                    dbFieldDescriptor* component = fd->components;
                    while (--nElems >= 0) {
                        component->fetchRecordFields(dstElem, srcElem);
                        dstElem += component->appSize;
                        srcElem += component->dbsSize;
                    }
                }
            }
            break;
          case dbField::tpReference:
            ((dbAnyReference*)(dst+fd->appOffs))->oid =
                *(oid_t*)(src+fd->dbsOffs);
            break;
          case dbField::tpRectangle:
            *(rectangle*)(dst+fd->appOffs) = *(rectangle*)(src+fd->dbsOffs);
            break;
          case dbField::tpStructure:
            fd->components->fetchRecordFields(dst + fd->appOffs, src);
            break;
          default:
            return;
        }
    } while ((fd = fd->next) != this);
}


size_t dbFieldDescriptor::storeRecordFields(byte* dst, byte* src, size_t offs, StoreMode mode)
{
    dbFieldDescriptor* fd = this;
    do {
#ifdef AUTOINCREMENT_SUPPORT
        if ((fd->indexType & AUTOINCREMENT) != 0) { 
            if (mode == Insert || (mode == Import && *(int4*)(src+fd->appOffs) == 0)) { 
                assert (fd->appType == dbField::tpInt4);
                *(int4*)(dst+fd->dbsOffs) = *(int4*)(src+fd->appOffs) = fd->defTable->autoincrementCount;
                continue;
            } else if (mode == Import) { 
                if (*(int4*)(src+fd->appOffs) > fd->defTable->autoincrementCount) {
                    fd->defTable->autoincrementCount = *(int4*)(src+fd->appOffs);
                } 
            }
        }
#endif
        switch (fd->appType) {
          case dbField::tpBool:
            *(bool*)(dst+fd->dbsOffs) = *(bool*)(src+fd->appOffs);
            break;
          case dbField::tpInt1:
            *(int1*)(dst+fd->dbsOffs) = *(int1*)(src+fd->appOffs);
            break;
          case dbField::tpInt2:
            *(int2*)(dst+fd->dbsOffs) = *(int2*)(src+fd->appOffs);
            break;
          case dbField::tpInt4:
            *(int4*)(dst+fd->dbsOffs) = *(int4*)(src+fd->appOffs);
            break;
          case dbField::tpInt8:
            *(db_int8*)(dst+fd->dbsOffs) = *(db_int8*)(src+fd->appOffs);
            break;
          case dbField::tpReal4:
            *(real4*)(dst+fd->dbsOffs) = *(real4*)(src+fd->appOffs);
            break;
          case dbField::tpReal8:
            *(real8*)(dst+fd->dbsOffs) = *(real8*)(src+fd->appOffs);
            break;
          case dbField::tpRawBinary:
            memcpy(dst+fd->dbsOffs, src+fd->appOffs, fd->dbsSize);
            break;
#ifdef USE_MFC_STRING
          case dbField::tpMfcString:
            { 
                offs = DOALIGN(offs, sizeof(MFC_STRING::XCHAR));
                MFC_STRING::PXSTR dstElem = (MFC_STRING::PXSTR)(dst+offs);
                ((dbVarying*)(dst+fd->dbsOffs))->offs = (int4)offs;
                MFC_STRING* str = (MFC_STRING*)(src + fd->appOffs);
                int len = str->GetLength();
                MFC_STRING::CopyChars(dstElem,*str,len);
                dstElem[len] = '\0';
                ((dbVarying*)(dst+fd->dbsOffs))->size = len+1;
                offs += (len+1)*sizeof(char_t);
            }
            break;
#endif
#ifdef USE_STD_STRING
          case dbField::tpStdString:
            { 
                offs = DOALIGN(offs, sizeof(char_t));
                char_t* dstElem = (char_t*)(dst+offs);
                ((dbVarying*)(dst+fd->dbsOffs))->offs = (int4)offs;
                STD_STRING* str = (STD_STRING*)(src + fd->appOffs);
                size_t len = str->length();
                str->copy(dstElem, len);
                dstElem[len] = '\0';
                ((dbVarying*)(dst+fd->dbsOffs))->size = (nat4)(len+1);
                offs += (len+1)*sizeof(char_t);
            }
            break;
#endif
          case dbField::tpString:
            {
                offs = DOALIGN(offs, sizeof(char_t));
                char_t* dstElem = (char_t*)(dst+offs);
                char_t* str = *(char_t**)(src + fd->appOffs);
                ((dbVarying*)(dst+fd->dbsOffs))->offs = (int4)offs;
                if (str == NULL) { 
                    *dstElem = 0;
                    offs += sizeof(char_t)*(((dbVarying*)(dst+fd->dbsOffs))->size = 1);
                } else {                     
                    STRCPY(dstElem, str);
                    offs += sizeof(char_t)*(((dbVarying*)(dst+fd->dbsOffs))->size = (nat4)STRLEN(str) + 1);
                }
            }
            break;
          case dbField::tpArray:
            {
                int nElems = (int)((dbAnyArray*)(src + fd->appOffs))->length();
                byte* srcElem=(byte*)((dbAnyArray*)(src+fd->appOffs))->base();
                offs = DOALIGN(offs, fd->components->alignment);
                byte* dstElem = dst+offs;
                ((dbVarying*)(dst+fd->dbsOffs))->size = nElems;
                ((dbVarying*)(dst+fd->dbsOffs))->offs = (int4)offs;
                size_t sizeElem = fd->components->dbsSize;
                size_t offsElem = nElems*sizeElem;
                offs += offsElem;
                if (fd->attr & dbFieldDescriptor::OneToOneMapping) {
                    memcpy(dstElem, srcElem, offsElem);
                } else {
                    dbFieldDescriptor* component = fd->components;
                    while (--nElems >= 0) {
                        offsElem =
                            component->storeRecordFields(dstElem, srcElem, offsElem, mode);
                        offsElem -= sizeElem;
                        dstElem += sizeElem;
                        srcElem += component->appSize;
                    }
                    offs += offsElem;
                }
            }
            break;
          case dbField::tpReference:
            *(oid_t*)(dst+fd->dbsOffs) = ((dbAnyReference*)(src+fd->appOffs))->oid;
            break;
          case dbField::tpRectangle:
            *(rectangle*)(dst+fd->dbsOffs) = *(rectangle*)(src+fd->appOffs);
            break;
          case dbField::tpStructure:
            offs = fd->components->storeRecordFields(dst, src+fd->appOffs, offs, mode);
            break;
          default:
            return offs;
        }
    } while ((fd = fd->next) != this);

    return offs;
}


void dbFieldDescriptor::markUpdatedFields(byte* dst, byte* src)
{
    dbFieldDescriptor* fd = this;
    do {
        if ((fd->indexType & (HASHED|INDEXED)) != 0/* && (fd->indexType & AUTOINCREMENT) == 0*/) {
            switch (fd->appType) {
              case dbField::tpBool:
                if (*(bool*)(dst+fd->dbsOffs) != *(bool*)(src+fd->appOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpInt1:
                if (*(int1*)(dst+fd->dbsOffs) != *(int1*)(src+fd->appOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpInt2:
                if (*(int2*)(dst+fd->dbsOffs) != *(int2*)(src+fd->appOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpInt4:
                if (*(int4*)(dst+fd->dbsOffs) != *(int4*)(src+fd->appOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpInt8:
                if (*(db_int8*)(dst+fd->dbsOffs) != *(db_int8*)(src+fd->appOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpReference:
                if (*(oid_t*)(dst+fd->dbsOffs) != *(oid_t*)(src+fd->appOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpRectangle:
                if (*(rectangle*)(dst+fd->dbsOffs) != *(rectangle*)(src+fd->appOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpReal4:
                if (*(real4*)(dst+fd->dbsOffs) != *(real4*)(src+fd->appOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpReal8:
                if (*(real8*)(dst+fd->dbsOffs) != *(real8*)(src+fd->appOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpRawBinary:
                if (memcmp(dst+fd->dbsOffs, src+fd->appOffs, fd->dbsSize) != 0) {
                    fd->attr |= Updated;
                }
                break;
#ifdef USE_MFC_STRING
              case dbField::tpMfcString:
                if (*(MFC_STRING*)(src + fd->appOffs) != (MFC_STRING::PCXSTR)(dst + ((dbVarying*)(dst+fd->dbsOffs))->offs)) {
                    fd->attr |= Updated;
                }
                break;
#endif          
#ifdef USE_STD_STRING
              case dbField::tpStdString:
                if (*(STD_STRING*)(src + fd->appOffs) != (char_t*)(dst + ((dbVarying*)(dst+fd->dbsOffs))->offs)) {
                    fd->attr |= Updated;
                }
                break;
#endif          
              case dbField::tpString:
                if (STRCMP((char_t*)(dst + ((dbVarying*)(dst+fd->dbsOffs))->offs),
                           *(char_t**)(src + fd->appOffs)) != 0)
                {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpStructure:
                fd->components->markUpdatedFields(dst, src+fd->appOffs);
                break;
              case dbField::tpArray:
                break;
              default:
                return;
            }
        }
    } while ((fd = fd->next) != this);
}

void dbFieldDescriptor::markUpdatedFields2(byte* dst, byte* src)
{
    dbFieldDescriptor* fd = this;
    do {
        if ((fd->indexType & (HASHED|INDEXED)) != 0/* && (fd->indexType & AUTOINCREMENT) == 0*/) {
            switch (fd->type) {
              case dbField::tpBool:
                if (*(bool*)(dst+fd->dbsOffs) != *(bool*)(src+fd->dbsOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpInt1:
                if (*(int1*)(dst+fd->dbsOffs) != *(int1*)(src+fd->dbsOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpInt2:
                if (*(int2*)(dst+fd->dbsOffs) != *(int2*)(src+fd->dbsOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpInt4:
                if (*(int4*)(dst+fd->dbsOffs) != *(int4*)(src+fd->dbsOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpInt8:
                if (*(db_int8*)(dst+fd->dbsOffs) != *(db_int8*)(src+fd->dbsOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpReference:
                if (*(oid_t*)(dst+fd->dbsOffs) != *(oid_t*)(src+fd->dbsOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpRectangle:
                if (*(rectangle*)(dst+fd->dbsOffs) != *(rectangle*)(src+fd->dbsOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpReal4:
                if (*(real4*)(dst+fd->dbsOffs) != *(real4*)(src+fd->dbsOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpReal8:
                if (*(real8*)(dst+fd->dbsOffs) != *(real8*)(src+fd->dbsOffs)) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpRawBinary:
                if (memcmp(dst+fd->dbsOffs, src+fd->dbsOffs, fd->dbsSize) != 0) {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpString:
                if (STRCMP((char_t*)(dst + ((dbVarying*)(dst+fd->dbsOffs))->offs),
                           (char_t*)(dst + ((dbVarying*)(src+fd->dbsOffs))->offs)) != 0)
                {
                    fd->attr |= Updated;
                }
                break;
              case dbField::tpStructure:
                fd->components->markUpdatedFields2(dst, src);
                break;
              case dbField::tpArray:
                break;
              default:
                return;
            }
        }
    } while ((fd = fd->next) != this);
}


size_t dbFieldDescriptor::convertRecord(byte* dst, byte* src, size_t offs)
{
    dbFieldDescriptor* fd = this;
    int1  i1;
    int2  i2;
    int4  i4;
    db_int8  i8;
    real4 f4;
    real8 f8;
    bool  b;
    do {
        switch (fd->type) {
          case dbField::tpBool:
            switch (fd->oldDbsType) {
              case dbField::tpBool:
                b = *(bool*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt1:
                b = *(int1*)(src + fd->oldDbsOffs) != 0;
                break;
              case dbField::tpInt2:
                b = *(int2*)(src + fd->oldDbsOffs) != 0;
                break;
              case dbField::tpInt4:
                b = *(int4*)(src + fd->oldDbsOffs) != 0;
                break;
              case dbField::tpInt8:
                b = *(db_int8*)(src + fd->oldDbsOffs) != 0;
                break;
              case dbField::tpReal4:
                b = *(real4*)(src + fd->oldDbsOffs) != 0;
                break;
              case dbField::tpReal8:
                b = *(real8*)(src + fd->oldDbsOffs) != 0;
                break;
              default:
                b = fd->defaultValue.Bool;
            }
            *(bool*)(dst + fd->dbsOffs) = b;
            break;

          case dbField::tpInt1:
            switch (fd->oldDbsType) {
              case dbField::tpBool:
                i1 = *(bool*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt1:
                i1 = *(int1*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt2:
                i1 = (int1)*(int2*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt4:
                i1 = (int1)*(int4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt8:
                i1 = (int1)*(db_int8*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal4:
                i1 = (int1)*(real4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal8:
                i1 = (int1)*(real8*)(src + fd->oldDbsOffs);
                break;
              default:
                i1 = fd->defaultValue.Int1;
            }
            *(int1*)(dst + fd->dbsOffs) = i1;
            break;

          case dbField::tpInt2:
            switch (fd->oldDbsType) {
              case dbField::tpBool:
                i2 = *(bool*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt1:
                i2 = *(int1*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt2:
                i2 = *(int2*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt4:
                i2 = (int2)*(int4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt8:
                i2 = (int2)*(db_int8*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal4:
                i2 = (int2)*(real4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal8:
                i2 = (int2)*(real8*)(src + fd->oldDbsOffs);
                break;
              default:
                i2 = fd->defaultValue.Int2;
            }
            *(int2*)(dst + fd->dbsOffs) = i2;
            break;

          case dbField::tpInt4:
            switch (fd->oldDbsType) {
              case dbField::tpBool:
                i4 = *(bool*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt1:
                i4 = *(int1*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt2:
                i4 = *(int2*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt4:
                i4 = *(int4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt8:
                i4 = (int4)*(db_int8*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal4:
                i4 = (int4)*(real4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal8:
                i4 = (int4)*(real8*)(src + fd->oldDbsOffs);
                break;
              default:
                i4 = fd->defaultValue.Int4;
            }
            *(int4*)(dst + fd->dbsOffs) = i4;
            break;

          case dbField::tpInt8:
            switch (fd->oldDbsType) {
              case dbField::tpBool:
                i8 = *(bool*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt1:
                i8 = *(int1*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt2:
                i8 = *(int2*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt4:
                i8 = *(int4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt8:
                i8 = *(db_int8*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal4:
                i8 = (db_int8)*(real4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal8:
                i8 = (db_int8)*(real8*)(src + fd->oldDbsOffs);
                break;
              default:
                i8 = fd->defaultValue.Db_Int8;
            }
            *(db_int8*)(dst + fd->dbsOffs) = i8;
            break;

          case dbField::tpReal4:
            switch (fd->oldDbsType) {
              case dbField::tpBool:
                f4 = (real4)*(bool*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt1:
                f4 = (real4)*(int1*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt2:
                f4 = (real4)*(int2*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt4:
                f4 = (real4)*(int4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt8:
                f4 = (real4)*(db_int8*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal4:
                f4 = *(real4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal8:
                f4 = (real4)*(real8*)(src + fd->oldDbsOffs);
                break;
              default:
                f4 = fd->defaultValue.Real4;
            }
            *(real4*)(dst + fd->dbsOffs) = f4;
            break;

          case dbField::tpReal8:
            switch (fd->oldDbsType) {
              case dbField::tpBool:
                f8 = (real8)*(bool*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt1:
                f8 = (real8)*(int1*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt2:
                f8 = (real8)*(int2*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt4:
                f8 = (real8)*(int4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpInt8:
                f8 = (real8)*(db_int8*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal4:
                f8 = *(real4*)(src + fd->oldDbsOffs);
                break;
              case dbField::tpReal8:
                f8 = *(real8*)(src + fd->oldDbsOffs);
                break;
              default:
                f8 = fd->defaultValue.Real8;
            }
            *(real8*)(dst + fd->dbsOffs) = f8;
            break;

          case dbField::tpRawBinary:
            if (fd->oldDbsType == dbField::tpRawBinary) {
                memcpy(dst + fd->dbsOffs, src + fd->oldDbsOffs,
                       size_t(fd->oldDbsSize) < fd->dbsSize
                       ? size_t(fd->oldDbsSize) : fd->dbsSize);
            }
            break;

          case dbField::tpString:
            if (fd->oldDbsType == dbField::tpUnknown) {
                ((dbVarying*)(dst + fd->dbsOffs))->size = 1;
                ((dbVarying*)(dst + fd->dbsOffs))->offs = (int4)offs;
                *(char_t*)(dst + offs) = 0;
                offs += sizeof(char_t);
            } else {
                size_t len =
                    ((dbVarying*)(src + fd->oldDbsOffs))->size;
                ((dbVarying*)(dst + fd->dbsOffs))->size = (nat4)len;
                ((dbVarying*)(dst + fd->dbsOffs))->offs = (int4)offs;
                memcpy(dst + offs,
                       src + ((dbVarying*)(src+fd->oldDbsOffs))->offs, len*sizeof(char_t));
                offs += len*sizeof(char_t);
            }
            break;

          case dbField::tpArray:
            if (fd->oldDbsType == dbField::tpUnknown) {
                ((dbVarying*)(dst + fd->dbsOffs))->size = 0;
                ((dbVarying*)(dst + fd->dbsOffs))->offs = 0;
            } else {
                int len = ((dbVarying*)(src+fd->oldDbsOffs))->size;
                byte* srcElem = src + ((dbVarying*)(src+fd->oldDbsOffs))->offs;
                ((dbVarying*)(dst + fd->dbsOffs))->size = len;
                offs = DOALIGN(offs, fd->components->alignment);
                byte* dstElem = dst+offs;
                ((dbVarying*)(dst+fd->dbsOffs))->offs = (int4)offs;
                size_t offsElem = len*fd->components->dbsSize;
                offs += offsElem;
                while (--len >= 0) {
                    offsElem = fd->components->convertRecord(dstElem, srcElem,
                                                             offsElem);
                    offsElem -= fd->components->dbsSize;
                    dstElem += fd->components->dbsSize;
                    srcElem += fd->components->oldDbsSize;
                }
                offs += offsElem;
            }
            break;

          case dbField::tpStructure:
            offs = fd->components->convertRecord(dst, src, offs);
            break;

          case dbField::tpReference:
            if (fd->oldDbsType == dbField::tpUnknown) {
                *(oid_t*)(dst + fd->dbsOffs) = 0;
            } else {
                *(oid_t*)(dst + fd->dbsOffs) = *(oid_t*)(src + fd->oldDbsOffs);
            }
            break;
          case dbField::tpRectangle:
            if (fd->oldDbsType == dbField::tpUnknown) {
                memset(dst + fd->dbsOffs, 0, sizeof(rectangle));
            } else {
                *(rectangle*)(dst + fd->dbsOffs) = *(rectangle*)(src + fd->oldDbsOffs);
            }
            break;
          default:
            return offs;
        }
    } while ((fd = fd->next) != this);

    return offs;
}



int dbTableDescriptor::initialAutoincrementCount;

dbTableDescriptor::dbTableDescriptor(char_t const*      tableName,
                                     dbDatabase*        database,
                                     size_t             objSize,
                                     describeFunc       func,
                                     dbTableDescriptor* original)
{
    cloneOf = original;
    if (original == NULL) {
        isStatic = true;
        link();
    } else {
        isStatic = false;
    }
    name = (char_t*)tableName;
    dbSymbolTable::add(name, tkn_ident, GB_CLONE_ANY_IDENTIFIER);
    describeComponentsFunc = func;
    columns = (*func)();
    nextFieldLink = &firstField;
    hashedFields = NULL;
    indexedFields = NULL;
    inverseFields = NULL;
    tableId = 0;
    nFields = 0;
    nColumns = 0;
    firstRow = 0;
    lastRow = 0;
    nRows = 0;
    db = database;
    fixedDatabase = database != NULL;
    fixedSize = sizeof(dbRecord);
    attr = dbFieldDescriptor::OneToOneMapping;
    appSize = 0;
    autoincrementCount = initialAutoincrementCount;
    size_t maxDbsAlignment, maxAppAlignment;
    calculateFieldsAttributes(columns, STRLITERAL(""),
                              sizeof(dbRecord),
                              HASHED|INDEXED, attr, 
                              maxDbsAlignment, maxAppAlignment);
#if CHECK_RECORD_SIZE
    appSize = DOALIGN(appSize, maxAppAlignment);
    if (appSize < objSize) {
        fprintf(stderr, "Warning: may be not all fields of the class '%s' "
                "were described\n", name);
    }
#endif
    *nextFieldLink = NULL;
    isInBatch = false;
}

struct _abool { char n; bool v; };
struct _aint1 { char n; int1 v; };
struct _aint2 { char n; int2 v; };
struct _aint4 { char n; int4 v; };
struct _aint8 { char n; db_int8 v; };
struct _areal4 { char n; real4 v; };
struct _areal8 { char n; real8 v; };
struct _astring { char n; char *v; };
#ifdef USE_STD_STRING
struct _astdstring { char n; STD_STRING v; };
#endif
#ifdef USE_MFC_STRING
struct _amfcstring { char n; MFC_STRING v; };
#endif
struct _areference { char n; oid_t v; };
struct _aarray { char n; void* v; };
struct _arectangle { char n; rectangle v; };

#define alignmentof(type) offsetof(_a##type, v)

const size_t NativeSizeOfType[] = 
{  
    sizeof(bool),    // tpBool
    sizeof(int1),    // tpInt1
    sizeof(int2),    // tpInt2
    sizeof(int4),    // tpInt4
    sizeof(db_int8), // tpInt8
    sizeof(real4),   // tpReal4
    sizeof(real8),   // tpReal8
    sizeof(char*),   // tpString
    sizeof(dbAnyReference), // tpReference
    sizeof(dbArray<byte>), // tpArray
    0, //  tpMethodBool
    0, //          tpMethodInt1,
    0, //          tpMethodInt2,
    0, //          tpMethodInt4,
    0, //          tpMethodInt8,
    0, //          tpMethodReal4,
    0, //          tpMethodReal8,
    0, //          tpMethodString,
    0, //          tpMethodReference,
    0, //          tpStructure,
    0, //          tpRawBinary, 
#ifdef USE_STD_STRING
    sizeof(std::string), //   tpStdString
#else
    0, //   tpStdString
#endif
#ifdef USE_MFC_STRING
    sizeof(MFC_STRING), //   tpMfcString
#else
    0, //   tpMfcString
#endif
    sizeof(rectangle), // tpRectangle
    0  //      tpUnknown
};


const size_t NativeAlignmentOfType[] = 
{  
    alignmentof(bool),    // tpBool
    alignmentof(int1),    // tpInt1
    alignmentof(int2),    // tpInt2
    alignmentof(int4),    // tpInt4
    alignmentof(int8), // tpInt8
    alignmentof(real4),   // tpReal4
    alignmentof(real8),   // tpReal8
    alignmentof(string),   // tpString
    alignmentof(reference), // tpReference
    alignmentof(array), // tpArray
    1, //  tpMethodBool
    1, //          tpMethodInt1,
    1, //          tpMethodInt2,
    1, //          tpMethodInt4,
    1, //          tpMethodInt8,
    1, //          tpMethodReal4,
    1, //          tpMethodReal8,
    1, //          tpMethodString,
    1, //          tpMethodReference,
    1, //          tpStructure,
    1, //          tpRawBinary, 
#ifdef USE_STD_STRING
    alignmentof(stdstring), //   tpStdString
#else
    1, //   tpStdString
#endif
#ifdef USE_MFC_STRING
    alignmentof(mfcstring), //   tpMfcString
#else
    1, //   tpMfcString
#endif
    alignmentof(rectangle), // tpRectangle
    1        // tpUnknown
};

void dbTableDescriptor::calculateFieldsAttributes(dbFieldDescriptor* first,
                                                  char_t const*      prefix,
                                                  int                offs,
                                                  int                indexMask,
                                                  int&               attr,
                                                  size_t&            dbsAlignment,
                                                  size_t&            appAlignment)
{
    dbFieldDescriptor *field = first;
    dbsAlignment = appAlignment = 1;
    do {
        if (field->method) {
            assert(((void)"Not empty record", field != first));
            do {
                assert(((void)"Methods should be specified after variables",
                        field->method != NULL));
                field->dbsOffs = first->dbsOffs;
                field->components = first;
                if (attr & dbFieldDescriptor::OneToOneMapping) {
                    field->method = field->method->optimize();
                }
            } while ((field = field->next) != first);
            break;
        }
        if (*prefix != '\0') {
            char_t* p = new char_t[STRLEN(prefix)+STRLEN(field->name)+1];
            SPRINTF(SPRINTF_BUFFER(p), STRLITERAL("%s%s"), prefix, field->name);
            field->longName = p;
        } else {
            nColumns += 1;
            field->longName = new char_t[STRLEN(field->name)+1];
            STRCPY(field->longName, field->name);
        }
        field->defTable = this;
        field->indexType &= indexMask|DB_FIELD_INHERITED_MASK;
        field->attr = (attr & dbFieldDescriptor::ComponentOfArray)
                    | dbFieldDescriptor::OneToOneMapping;
        if (field->inverseRefName || (field->indexType & DB_BLOB_CASCADE_DELETE)) {
            assert(!(attr & dbFieldDescriptor::ComponentOfArray)
                   && (field->type == dbField::tpReference
                       || (field->type == dbField::tpArray
                           && field->components->type==dbField::tpReference)));
            field->nextInverseField = inverseFields;
            inverseFields = field;
        }
        *nextFieldLink = field;
        nextFieldLink = &field->nextField;
        field->fieldNo = (int)(nFields++);

        size_t dbsFieldAlignment = 1;
        size_t appFieldAlignment = 1;

        switch (field->type) {
          case dbField::tpArray:
            {
                size_t saveOffs = fixedSize;
                size_t saveAppSize = appSize;
                size_t dbsElemAlignment, appElemAlignment;
                fixedSize = 0;
                attr = (attr | dbFieldDescriptor::HasArrayComponents)
                     & ~dbFieldDescriptor::OneToOneMapping;
                field->attr |= dbFieldDescriptor::ComponentOfArray;
                calculateFieldsAttributes(field->components, field->longName,
                                          0, 0, field->attr,
                                          dbsElemAlignment, appElemAlignment);
                if (field->components->dbsSize != field->components->appSize) {
                    field->attr &= ~dbFieldDescriptor::OneToOneMapping;
                }
                fixedSize = saveOffs;
                appSize = DOALIGN(saveAppSize, sizeof(void*)) + sizeof(void*)*3;
                dbsFieldAlignment = 4;
                appFieldAlignment = sizeof(void*);
                break;
            }
          case dbField::tpStructure:
            {
                char_t* aggregateName = new char_t[STRLEN(field->longName) + 2];
                SPRINTF(SPRINTF_BUFFER(aggregateName), STRLITERAL("%s."), field->longName);
                size_t saveOffs = fixedSize;
                size_t saveAppSize = appSize;
                appSize = 0;
                calculateFieldsAttributes(field->components,
                                          aggregateName,
                                          offs + field->appOffs,
                                          field->indexType,
                                          field->attr,
                                          dbsFieldAlignment, appFieldAlignment);
                field->alignment = dbsFieldAlignment;
                field->dbsOffs = field->components->dbsOffs;
                attr |= field->attr & dbFieldDescriptor::HasArrayComponents;
                attr &= field->attr | ~dbFieldDescriptor::OneToOneMapping;
                field->dbsSize = DOALIGN(fixedSize-saveOffs, dbsFieldAlignment);
                if ((field->attr & dbFieldDescriptor::HasArrayComponents)
                    && appFieldAlignment < sizeof(void*))
                {
                    appFieldAlignment = sizeof(void*);
                }
                appSize = DOALIGN(appSize, appFieldAlignment)
                        + DOALIGN(saveAppSize, appFieldAlignment);
                delete[] aggregateName;
                break;
            }
          case dbField::tpStdString:
          case dbField::tpMfcString:
          case dbField::tpString:
            attr = (attr | dbFieldDescriptor::HasArrayComponents)
                & ~dbFieldDescriptor::OneToOneMapping;
            // no break
          default:
            appFieldAlignment = NativeAlignmentOfType[field->appType];
            dbsFieldAlignment = field->alignment;
            appSize = DOALIGN(appSize, appFieldAlignment) + field->appSize;
        }
        if (dbsAlignment < dbsFieldAlignment) { 
            dbsAlignment = dbsFieldAlignment;
        }
        if (appAlignment < appFieldAlignment) { 
            appAlignment = appFieldAlignment;
        }
        if (field->type != dbField::tpStructure) {
            field->dbsOffs = (int)(fixedSize = DOALIGN(fixedSize, dbsFieldAlignment));
            fixedSize += field->dbsSize;
            if (field->dbsOffs != offs + field->appOffs) {
                attr &= ~dbFieldDescriptor::OneToOneMapping;
            }

            if (field->indexType & (HASHED|INDEXED)) {
                assert(!(field->attr & dbFieldDescriptor::ComponentOfArray));
#if 1 // hash table is currently not implemted for this veriosn
      // use Btree instead of hash table
                if (field->indexType & HASHED) {
                    field->indexType |= INDEXED;
                    field->indexType &= ~HASHED;
                }
#else
                if (field->indexType & HASHED) {
                    field->nextHashedField = hashedFields;
                    hashedFields = field;
                }
#endif
                if (field->indexType & INDEXED) {
                    field->nextIndexedField = indexedFields;
                    indexedFields = field;
                }
            }
        }
    } while ((field = field->next) != first);
}


int dbFieldDescriptor::sizeWithoutOneField(dbFieldDescriptor* field,
                                           byte* base, size_t& size)
{
    dbFieldDescriptor* fd = this;
    int offs, last = 0;
    do {
        if (fd != field) {
            if (fd->type == dbField::tpArray || fd->type == dbField::tpString){
                dbVarying* arr = (dbVarying*)(base + fd->dbsOffs);
                int n = arr->size;
                if (arr->offs > last && n > 0) {
                    last = arr->offs;
                }
                size = DOALIGN(size, fd->components->alignment)
                     + fd->components->dbsSize * n;
                if (fd->attr & HasArrayComponents) {
                    byte* elem = base + arr->offs;
                    while (--n >= 0) {
                        offs = fd->components->sizeWithoutOneField(field,
                                                                   elem, size);
                        if (arr->offs + offs > last) {
                            last = arr->offs + offs;
                        }
                        elem += fd->components->dbsSize;
                    }
                }
            } else if (fd->attr & HasArrayComponents) {
                offs = fd->components->sizeWithoutOneField(field, base, size);
                if (offs > last) {
                    last = offs;
                }
            }
        }
    } while ((fd = fd->next) != this);

    return last;
}


size_t dbFieldDescriptor::copyRecordExceptOneField(dbFieldDescriptor* field,
                                                   byte* dst, byte* src,
                                                   size_t offs)
{
    dbFieldDescriptor* fd = this;
    do {
        if (fd != field) {
            if (fd->type == dbField::tpArray || fd->type == dbField::tpString){
                dbVarying* srcArr = (dbVarying*)(src + fd->dbsOffs);
                dbVarying* dstArr = (dbVarying*)(dst + fd->dbsOffs);
                int n = srcArr->size;
                byte* srcElem = src + srcArr->offs;
                offs = DOALIGN(offs, fd->components->alignment);
                byte* dstElem = dst + offs;
                dstArr->offs = (int4)offs;
                dstArr->size = n;
                size_t sizeElem = fd->components->dbsSize;
                size_t offsElem = sizeElem * n;
                offs += offsElem;
                if (fd->attr & HasArrayComponents) {
                    while (--n >= 0) {
                        offsElem = fd->components->
                            copyRecordExceptOneField(field, dstElem, srcElem,
                                                     offsElem);
                        offsElem -= sizeElem;
                        dstElem += sizeElem;
                        srcElem += sizeElem;
                    }
                    offs += offsElem;
                } else {
                    memcpy(dstElem, srcElem, offsElem);
                }
            } else if (fd->attr & HasArrayComponents) {
                offs = fd->components->copyRecordExceptOneField(field, dst,
                                                                src, offs);
            } else if (fd->method == NULL) {
                memcpy(dst+fd->dbsOffs, src+fd->dbsOffs, fd->dbsSize);
            }
        }
    } while ((fd = fd->next) != this);

    return offs;
}


bool dbTableDescriptor::checkRelationship() 
{
    dbFieldDescriptor* fd;
    bool result = true;
    for (fd = inverseFields; fd != NULL; fd = fd->nextInverseField) {
        if (!(fd->indexType & DB_BLOB_CASCADE_DELETE)) { 
            char msg[MAX_MSG_SIZE];
            dbTableDescriptor* refTable =
                fd->refTable ? fd->refTable : fd->components != NULL ? fd->components->refTable : NULL;
            if (refTable == NULL) { 
                result = false;
                sprintf(msg, "Failed to locate referenced table %" STRING_FORMAT, 
                        fd->refTableName != NULL ? fd->refTableName : fd->components != NULL ? fd->components->refTableName : _T("<?>"));
                db->handleError(dbDatabase::InconsistentInverseReference, msg);
            } else { 
                fd->inverseRef = refTable->findSymbol(fd->inverseRefName);
                if (fd->inverseRef == NULL || fd->inverseRef->inverseRefName != fd->name) { 
                    result = false;
                    if (fd->inverseRef == NULL) { 
                        sprintf(msg, "Failed to locate inverse reference field %" STRING_FORMAT ".%" STRING_FORMAT " for field %" STRING_FORMAT ".%" STRING_FORMAT, 
                                refTable->name, fd->inverseRefName, fd->defTable->name, fd->longName);
                    } else {
                        sprintf(msg, "Inverse references for field %" STRING_FORMAT ".%" STRING_FORMAT " is %" STRING_FORMAT ".%" STRING_FORMAT ", but its inverse reference is %" STRING_FORMAT, 
                                fd->defTable->name, fd->longName, refTable->name, fd->inverseRefName, fd->inverseRef->inverseRefName);
                    }
                    db->handleError(dbDatabase::InconsistentInverseReference, msg);
                }                                
            }
        }
    }
    return result;
}

dbFieldDescriptor* dbTableDescriptor::find(char_t const* name)
{
    char_t* symnam = (char_t*)name;
    dbSymbolTable::add(symnam, tkn_ident, GB_CLONE_ANY_IDENTIFIER);
    return findSymbol(symnam);
}

dbFieldDescriptor* dbTableDescriptor::findSymbol(const char_t* name)
{
    dbFieldDescriptor* first = columns;
    dbFieldDescriptor* field = first;
    do {
        if (field->name == name) {
            return field;
        }
    } while ((field = field->next) != first);
    return NULL;
}

dbFieldDescriptor& dbFieldDescriptor::adjustOffsets(size_t offs)
{
    if (offs != 0) {
        dbFieldDescriptor* fd = this;
        do {
          fd->appOffs += (int)offs;
        } while ((fd = fd->next) != this);
    }
    return *this;
}




bool dbTableDescriptor::match(dbTable* table, bool confirmDeleteColumns, bool preserveExistedIndices, bool isEmpty)
{
    unsigned nFields = table->fields.size;
    unsigned nMatches = 0;
    bool formatNotChanged = (nFields == this->nFields);

    for (dbFieldDescriptor* fd = firstField; fd != NULL; fd = fd->nextField) {
        dbField* field = (dbField*)((char_t*)((byte*)table + table->fields.offs));
        fd->oldDbsType = dbField::tpUnknown;
        for (int n = nFields; --n >= 0; field++) {
            if (STRCMP(fd->longName, (char_t*)((byte*)field + field->name.offs)) == 0) {
                if (!isEmpty) { 
                    if (!((fd->type == dbField::tpReference
                           && field->type == dbField::tpReference
                           &&  (fd->refTable == NULL
                                || STRCMP((char_t*)((byte*)field + field->tableName.offs),
                                          fd->refTable->name) == 0))
                          || (fd->type <= dbField::tpReal8
                              && field->type <= dbField::tpReal8)
                          || (fd->type == dbField::tpString
                              && field->type == dbField::tpString)
                          || (fd->type == dbField::tpRectangle
                              && field->type == dbField::tpRectangle)
                          || (fd->type >= dbField::tpArray
                              && fd->type == field->type)))
                    {
                        db->handleError(dbDatabase::IncompatibleSchemaChange);
                    }
                    fd->oldDbsType = field->type;
                    fd->oldDbsOffs = field->offset;
                    fd->oldDbsSize = field->size;
                }
                if (field->type != fd->type || field->offset != fd->dbsOffs) {
                    formatNotChanged = false;
                }
                nMatches += 1;
                //
                // Try to reuse indices
                //
                fd->hashTable = 0;
                fd->bTree = 0;

                if (field->type == fd->type) {
                    if (((fd->indexType & HASHED) || preserveExistedIndices) && field->hashTable != 0) {
                        fd->hashTable = field->hashTable; // reuse index
                    }
                    if (((fd->indexType & INDEXED) || preserveExistedIndices) && field->bTree != 0) {
                        fd->bTree = field->bTree; // reuse index
                    }
                }
                break;
            }
        }
    }
    if (!confirmDeleteColumns) {             
        assert(((void)"field can be removed only from empty table",
                nFields==nMatches));
    }
    return formatNotChanged;
}

void dbTableDescriptor::setFlags() { 
    for (dbFieldDescriptor* fd = firstField; fd != NULL; fd = fd->nextField) {
        if (fd->bTree != 0) { 
            fd->indexType |= INDEXED;
        } else if (fd->hashTable != 0) { 
            fd->indexType |= HASHED;
        }
    }
}



bool dbTableDescriptor::equal(dbTable* table, bool ignoreIndices)
{
#ifdef AUTOINCREMENT_SUPPORT
    autoincrementCount = table->count;
#endif
    firstRow = table->firstRow;
    lastRow = table->lastRow;
    nRows = table->nRows;

    if (nColumns != table->nColumns ||
        nFields != table->fields.size || fixedSize != table->fixedSize)
    {
        return false;
    }
    dbField* field = (dbField*)((byte*)table + table->fields.offs);

    for (dbFieldDescriptor* fd = firstField; fd != NULL; fd = fd->nextField) {
        if (STRCMP(fd->longName, (char_t*)((byte*)field + field->name.offs)) != 0
            || (!fd->refTable && *((char_t*)((byte*)field + field->tableName.offs)) != '\0')
            || (fd->refTable && STRCMP((char_t*)((byte*)field + field->tableName.offs),
                                       fd->refTable->name) != 0)
            || (fd->inverseRefName == NULL
                && *(char_t*)((byte*)field + field->inverse.offs) != '\0')
            || (fd->inverseRefName != NULL
                && STRCMP((char_t*)((byte*)field + field->inverse.offs),
                          fd->inverseRefName) != 0)
            || fd->dbsOffs != field->offset
#ifndef OLD_FIELD_DESCRIPTOR_FORMAT
            || (fd->indexType != (int)field->flags && !ignoreIndices)
#endif
            || fd->type != field->type)
        {
            return false;
        }
        if (field->bTree != 0) { 
            fd->bTree = field->bTree;
        }
        if (field->hashTable != 0) { 
            fd->hashTable = field->hashTable;
        }
        field += 1;
    }
    return true;
}

dbMutex& dbTableDescriptor::getChainMutex() 
{
    if (chainMutex == NULL) { 
        chainMutex = new dbMutex();
    }
    return *chainMutex;
}

void dbTableDescriptor::link()
{
    dbCriticalSection cs(getChainMutex());
    next = chain;
    chain = this;
}
    
void dbTableDescriptor::unlink()
{
    dbCriticalSection cs(getChainMutex());
    dbTableDescriptor **tpp;
    for (tpp = &chain; *tpp != this; tpp = &(*tpp)->next);
    *tpp = next;
}

dbTableDescriptor::dbTableDescriptor(dbTable* table)
{
    isStatic = false;
    name = (char_t*)((byte*)table + table->name.offs);
    dbSymbolTable::add(name, tkn_ident, true);
    nextFieldLink = &firstField;
    hashedFields = NULL;
    indexedFields = NULL;
    inverseFields = NULL;
    nFields = 0;
    nColumns = 0;
    fixedSize = table->fixedSize;
    attr = dbFieldDescriptor::OneToOneMapping;
    appSize = 0;
    columns = buildFieldsList(table, STRLITERAL(""), 0, attr);
    *nextFieldLink = NULL;
    db = NULL;
    tableId = 0;
    firstRow = table->firstRow;
    lastRow = table->lastRow;
    nRows = table->nRows;
    cloneOf = NULL;
    isInBatch = false;
#ifdef AUTOINCREMENT_SUPPORT
    autoincrementCount = table->count;
#endif    
}



dbFieldDescriptor* dbTableDescriptor::buildFieldsList(dbTable*      table,
                                                      char_t const* prefix,
                                                      int           prefixLen,
                                                      int&          attr)
{
    dbFieldDescriptor* components = NULL;
    dbField* field = (dbField*)((byte*)table+table->fields.offs) + nFields;

    while (nFields < table->fields.size
           && STRNCMP((char_t*)((byte*)field + field->name.offs), 
                      prefix, prefixLen) == 0)
    {
        char_t* longName = (char_t*)((byte*)field + field->name.offs);
        char_t* name = longName + prefixLen;
        if (*name == '.') {
            name += 1;
        } else if (prefixLen != 0 && *name != '[') { 
            break;
        }
        dbSymbolTable::add(name, tkn_ident, true);
        dbFieldDescriptor* fd = new dbFieldDescriptor(name);
        fd->dbsOffs = field->offset;
        fd->alignment = fd->dbsSize = field->size;
        fd->longName = new char_t[STRLEN(longName)+1];
        STRCPY(fd->longName, longName);
        fd->type = fd->appType = field->type;
        fd->indexType = field->flags;
        size_t appFieldSize = NativeSizeOfType[field->type];
        size_t appFieldAlignment = NativeAlignmentOfType[field->type];

        if (field->type == dbField::tpRawBinary) {
            appFieldSize = field->size;
        }
        fd->appOffs = (int)(appSize = DOALIGN(appSize, appFieldAlignment));
        appSize += fd->appSize = appFieldSize;

        if ((fd->hashTable = field->hashTable) != 0) {
            fd->nextHashedField = hashedFields;
            hashedFields = fd;
        }
        if ((fd->bTree = field->bTree) != 0 || (fd->indexType & INDEXED)) {
            fd->nextIndexedField = indexedFields;
            indexedFields = fd;
        }
        fd->fieldNo = (int)(nFields++);
        fd->defTable = this;
        fd->refTable = NULL;
        fd->refTableName = NULL;
        if (field->hashTable != 0) { 
            fd->indexType |= HASHED;
        }
        if (field->bTree != 0) { 
            fd->indexType |= INDEXED;
        }
        if (field->tableName.size > 1) {
            fd->refTableName = (char_t*)((byte*)field + field->tableName.offs);
            dbSymbolTable::add(fd->refTableName, tkn_ident, true);
        }
        fd->inverseRefName = NULL;
        if (field->inverse.size > 1) {
            fd->nextInverseField = inverseFields;
            inverseFields = fd;
            fd->inverseRefName = (char_t*)((byte*)field + field->inverse.offs);
            dbSymbolTable::add(fd->inverseRefName, tkn_ident, true);
        }
        fd->attr = (attr & dbFieldDescriptor::ComponentOfArray) | dbFieldDescriptor::OneToOneMapping;

        *nextFieldLink = fd;
        nextFieldLink = &fd->nextField;

        if (prefixLen == 0) {
            nColumns += 1;
        }
        if (components == NULL) {
            components = fd;
        } else {
            fd->next = components;
            fd->prev = components->prev;
            components->prev->next = fd;
            components->prev = fd;
        }
        if (fd->type == dbField::tpArray || fd->type == dbField::tpString) {
            attr |= dbFieldDescriptor::HasArrayComponents;
            fd->attr |= dbFieldDescriptor::ComponentOfArray;
            fd->alignment = 4;
        }
        if (fd->type == dbField::tpArray || fd->type == dbField::tpStructure) {
            size_t saveAppSize = appSize;
            appSize = 0;
            fd->components =
                buildFieldsList(table, longName, (int)STRLEN(longName), fd->attr);
            attr |= fd->attr & dbFieldDescriptor::HasArrayComponents;
            attr &= fd->attr | ~dbFieldDescriptor::OneToOneMapping;
            field = (dbField*)((byte*)table + table->fields.offs) + nFields;
            if (fd->type == dbField::tpStructure) {
                size_t dbsAlignment = 1;
                size_t appAlignment = 1;
                dbFieldDescriptor* component = fd->components;
                do {
                    if (component->alignment > dbsAlignment) {
                        dbsAlignment = component->alignment;
                    }
                    size_t componentAppAlignment = NativeAlignmentOfType[component->type];
                    if (componentAppAlignment > appAlignment) {
                        appAlignment = componentAppAlignment;
                    }   
                } while ((component = component->next) != fd->components);
                fd->alignment = dbsAlignment;
                fd->appSize = appSize = DOALIGN(appSize, appAlignment);
                fd->appOffs = (int)(saveAppSize = DOALIGN(saveAppSize, appAlignment));
                appSize += saveAppSize;
            } else { 
                appSize = saveAppSize;
                switch (fd->components->type) { 
                  case dbField::tpString:
                    fd->arrayAllocator = &dbArray<char*>::arrayAllocator;
                    fd->attr &= ~dbFieldDescriptor::OneToOneMapping;
                    break;
                  case dbField::tpBool:
                    fd->arrayAllocator = &dbArray<bool>::arrayAllocator;
                    break;
                  case dbField::tpInt1:
                    fd->arrayAllocator = &dbArray<int1>::arrayAllocator;
                    break;
                  case dbField::tpInt2:
                    fd->arrayAllocator = &dbArray<int2>::arrayAllocator;
                    break;
                  case dbField::tpInt4:
                    fd->arrayAllocator = &dbArray<int4>::arrayAllocator;
                    break;
                  case dbField::tpInt8:
                    fd->arrayAllocator = &dbArray<db_int8>::arrayAllocator;
                    break;
                  case dbField::tpReal4:
                    fd->arrayAllocator = &dbArray<real4>::arrayAllocator;
                    break;
                  case dbField::tpReal8:
                    fd->arrayAllocator = &dbArray<real8>::arrayAllocator;
                    break;
                  case dbField::tpReference:
                    fd->arrayAllocator = &dbArray<dbAnyReference>::arrayAllocator;
                    break;
                  default:
                    fd->arrayAllocator = &dbAnyArray::arrayAllocator;
                    break;
                }
            }
        } else {
            if (fd->type == dbField::tpString) {
                fd->components = new dbFieldDescriptor(STRLITERAL("[]"));
                fd->components->type = fd->components->appType = dbField::tpInt1 + (sizeof(char_t) - 1);
                fd->components->dbsSize = fd->components->appSize =
                    fd->components->alignment = sizeof(char_t);
            }
            field += 1;
        }
    }
    return components;
}



size_t dbTableDescriptor::totalNamesLength()
{
    dbFieldDescriptor* fd;
    size_t len = STRLEN(name) + 1;
    for (fd = firstField; fd != NULL; fd = fd->nextField) {
        if (fd->name != NULL) {
            len += STRLEN(fd->longName) + 3;
            if (fd->inverseRefName != NULL) {
                len += STRLEN(fd->inverseRefName);
            }
            if (fd->refTable != NULL) {
                len += STRLEN(fd->refTable->name);
            } else if (fd->refTableName != NULL) {
                len += STRLEN(fd->refTableName);
            }
        }
    }
    return len;
}

dbTableDescriptor* dbTableDescriptor::clone()
{
    return new dbTableDescriptor(name, 
                                 DETACHED_TABLE, 
                                 appSize, 
                                 describeComponentsFunc,
                                 this);
}

dbTableDescriptor::~dbTableDescriptor()
{
    if (isStatic) { 
        unlink();
    }   
    dbFieldDescriptor* last = columns->prev;
    while (last->method != NULL) { 
        dbFieldDescriptor* prev = last->prev;
        delete last->method;
        delete last;
        if (last == columns) { 
            break;
        }
        last = prev;
    }
    dbFieldDescriptor *field, *nextField;       
    for (field = firstField; field != NULL; field = nextField) {
        nextField = field->nextField;
        delete field;
    }
}

void dbTableDescriptor::cleanup()
{
    delete chainMutex;                               
    chainMutex = NULL;
}

void dbTableDescriptor::storeInDatabase(dbTable* table)
{
    size_t offs = sizeof(dbTable) + sizeof(dbField)*nFields;
    table->name.offs = (int4)offs;
    table->name.size = (nat4)STRLEN(name)+1;
    STRCPY((char_t*)((byte*)table + offs), name);
    offs += table->name.size*sizeof(char_t);
    table->fields.offs = sizeof(dbTable);
    table->fields.size = (nat4)nFields;
    table->nRows = (nat4)nRows;
    table->nColumns = (nat4)nColumns;
    table->fixedSize = (nat4)fixedSize;
    table->firstRow = firstRow;
    table->lastRow = lastRow;
#ifdef AUTOINCREMENT_SUPPORT
    table->count = autoincrementCount;
#endif
    dbFieldDescriptor* fd;
    dbField* field = (dbField*)((byte*)table + table->fields.offs);
    offs -= sizeof(dbTable);
    for (fd = firstField; fd != NULL; fd = fd->nextField) {
        field->name.offs = (int4)offs;
        field->name.size = (nat4)(STRLEN(fd->longName) + 1);
        STRCPY((char_t*)((byte*)field + offs), fd->longName);
        offs += field->name.size*sizeof(char_t);
        field->tableName.offs = (int4)offs;
        if (fd->refTable != NULL) {
            field->tableName.size = (nat4)(STRLEN(fd->refTable->name) + 1);
            STRCPY((char_t*)((byte*)field + offs), fd->refTable->name);
        } else if (fd->refTableName != NULL) {
            field->tableName.size = (nat4)(STRLEN(fd->refTableName) + 1);
            STRCPY((char_t*)((byte*)field + offs), fd->refTableName);
        } else {
            field->tableName.size = 1;
            *((char_t*)((byte*)field + offs)) = '\0';
        }
        offs += field->tableName.size*sizeof(char_t);

        field->inverse.offs = (int4)offs;
        if (fd->inverseRefName != NULL) {
            field->inverse.size = (nat4)(STRLEN(fd->inverseRefName) + 1);
            STRCPY((char_t*)((byte*)field + offs), fd->inverseRefName);
        } else {
            field->inverse.size = 1;
            *((char_t*)((byte*)field + offs)) = '\0';
        }
        offs += field->inverse.size*sizeof(char_t);

        field->bTree = fd->bTree;
        field->hashTable = fd->hashTable;
        field->type = fd->type;
        field->size = (nat4)fd->dbsSize;
        field->offset = fd->dbsOffs;
        field->flags = fd->indexType;
        field += 1;
        offs -= sizeof(dbField);
    }
}

void* dbAnyMethodTrampoline::operator new(size_t size)
{
    void* p = dbMalloc(size);
    if (p == NULL) { 
        fprintf(stderr, "Malloc failed for size %ld\n", (long)size);
    }
    return p;
}

void  dbAnyMethodTrampoline::operator delete(void* p)
{
    dbFree(p);
}

dbAnyMethodTrampoline::~dbAnyMethodTrampoline() {}

END_GIGABASE_NAMESPACE

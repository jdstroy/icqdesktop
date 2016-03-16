//-< CLASS.H >-------------------------------------------------------*--------*
// GigaBASE                  Version 1.0         (c) 1999  GARRET    *     ?  *
// (Post Relational Database Management System)                      *   /\|  *
//                                                                   *  /  \  *
//                          Created:     20-Nov-98    K.A. Knizhnik  * / [] \ *
//                          Last update: 26-Nov-2001  K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Metaclass information
//-------------------------------------------------------------------*--------*

#ifndef __CLASS_H__
#define __CLASS_H__

#include "stdtp.h"
#include "sync.h"
#include "rectangle.h"

BEGIN_GIGABASE_NAMESPACE

#ifndef dbDatabaseOffsetBits
#ifdef LARGE_DATABASE_SUPPORT
#define dbDatabaseOffsetBits 40 // up to 1 terabyte
#else
#define dbDatabaseOffsetBits 32 // 37 - 128Gb, 40 - up to 1 terabyte
#endif
#endif

#ifndef dbDatabaseOidBits
#define dbDatabaseOidBits 32
#endif

/**
 * Object indentifier type
 */
#if dbDatabaseOidBits > 32
typedef nat8 oid_t;
#else
typedef nat4 oid_t;
#endif

/**
 * Object offset in the file type
 */
#if dbDatabaseOffsetBits > 32
typedef nat8 offs_t;
typedef db_int8 soffs_t;
#else
typedef nat4 offs_t;
typedef int4 soffs_t;
#endif

#include "selection.h"

/**
 * Types of field index
 */
enum dbIndexType {
    HASHED  = 1,                   // hash table
    INDEXED = 2,                   // B-tree
    CASE_INSENSITIVE = 4,          // Index is case insensitive

    DB_FIELD_CASCADE_DELETE = 8,   // Used by OWNER macro, do not set it explicitly
    UNIQUE = 16,                   // should be used in conjunction with HASHED or INDEXED - unique constraint 

    AUTOINCREMENT = 32,            // field is assigned automaticall incremented value
    OPTIMIZE_DUPLICATES = 64,      // index with lot of duplicate key values
    DB_BLOB_CASCADE_DELETE = 128,  // Used by BLOB macro to mark BLOB fields which should be deleted together with 
                                   // containing them record
    DB_TIMESTAMP = 256,            // field is used as timestamp (this flag is used by CLI to perfrom proper mapping,
                                   // it is not used by C++ API)
    DB_FIELD_INHERITED_MASK = ~(HASHED|INDEXED)
};

/**
 * Macros for describing indexed fields
 */
#define KEY(x, index) \
    *GB_NS::dbDescribeField(new GB_NS::dbFieldDescriptor(STRLITERAL(#x), (char*)&x-(char*)this, \
                                                         sizeof(x), index), x)

#define KEY_WITH_DEFAULT(x, index, defaultValue) \
	*GB_NS::dbDescribeField(new GB_NS::dbFieldDescriptor(STRLITERAL(#x), (char*)&x-(char*)this, \
	sizeof(x), index), x, defaultValue)

/**
 * Macros for describing non-indexed fields
 */
#define FIELD(x) KEY(x, 0)

#define FIELD_WITH_DEFAULT(x, defaultValue) KEY_WITH_DEFAULT(x, 0, defaultValue)


/**
 * Comparator for user defined raw binary fields
 */
typedef int (*dbUDTComparator)(void*, void*, size_t);

/**
 * Macro used to describe indexed raw binary fields with user-defined comparator
 */
#define UDT(x, index, comparator) \
    *GB_NS::dbDescribeRawField(new GB_NS::dbFieldDescriptor(STRLITERAL(#x), (char*)&x-(char*)this, \
                                                            sizeof(x), index), (GB_NS::dbUDTComparator)comparator)

/**
 * Macro used to describe raw binary field
 */
#define RAWFIELD(x) UDT(x, 0, &memcmp)

/**
 * Macro used to describe indexed raw binary field
 */
#define RAWKEY(x, index) UDT(x, index, &memcmp)


/**
 * Macro for describing relations between two tables. 
 * <code>x</code> should specify name of reference or array of reference field in this table, 
 * and <code>inverse</code> - field in the referenced table contining inverse reference.
 */
#define RELATION(x,inverse) \
    *GB_NS::dbDescribeField(new GB_NS::dbFieldDescriptor(STRLITERAL(#x), (char*)&x-(char*)this, \
                                                         sizeof(x), 0, STRLITERAL(#inverse)), x)

/**
 * Macro for describing relations between two tables.  
 * <code>x</code> should specify name of reference field in this table for which index will be created, 
 * and <code>inverse</code> - field in the referenced table contining inverse reference.
 */
#define INDEXED_RELATION(x,inverse) \
    *GB_NS::dbDescribeField(new GB_NS::dbFieldDescriptor(STRLITERAL(#x), (char*)&x-(char*)this, \
                                                         sizeof(x), GB_NS::INDEXED, STRLITERAL(#inverse)), x)
 
/**
 * Macro used to define relation owner (when owner is deleted, all referenced
 * members are also deleted). Members of of this relation should use 
 * <code>RELATION</code> macro to describe relation with owner.  
 */
#define OWNER(x,member) \
    *GB_NS::dbDescribeField(new GB_NS::dbFieldDescriptor(STRLITERAL(#x), (char*)&x-(char*)this, \
                                                         sizeof(x), GB_NS::DB_FIELD_CASCADE_DELETE, \
                                                         STRLITERAL(#member)), x)

/**
 * Macro used to describe method of the class which can be invoked from SubSQL
 */
#define METHOD_(x) \
    *GB_NS::dbDescribeMethod(new GB_NS::dbFieldDescriptor(STRLITERAL(#x)), &self::x)

/**
 * Macro for describing BLOB fields belonging only to one record. Actually BLOB in GigaBASE is implemeted as reference
 * to BLOB body. It is possible to refer the same BLOB from several records. That is why when record is removed, 
 * BLOBs referenced by this record are not removed. But in most typical case BLOB belongs only to one record.
 * And it will be convinient of its removed togehter with this record. To describe such BLOB fields BLOB macro should be used.
 */
#define BLOB(x) \
    *GB_NS::dbDescribeField(new GB_NS::dbFieldDescriptor(STRLITERAL(#x), (char*)&x-(char*)this, \
                                                         sizeof(x), GB_NS::DB_BLOB_CASCADE_DELETE), x)
/**
 * Macro used to describe superclass for this class
 */
#define SUPERCLASS(x) \
    x::dbDescribeComponents(NULL)->adjustOffsets((char*)((x*)this)-(char*)this)

/**
 * Macro used to describe fields of the record. Use <code>FIELD, KEY</code>...
 * macros separated by comma inside this macro to describe all fields of the record
 */
#define TYPE_DESCRIPTOR(fields) \
    GB_NS::dbFieldDescriptor* dbDescribeComponents(GB_NS::dbFieldDescriptor*) { \
        return &fields; \
    } \
    static GB_NS::dbTableDescriptor dbDescriptor

/**
 * Macro used to describe class, the only difference from <code>TYPE_DESCRIPTOR</code>
 * is that name of the class should be specified. This name is needed if you want
 * to describe methods.
 */
#define CLASS_DESCRIPTOR(name, fields) \
    typedef name self; \
    GB_NS::dbFieldDescriptor* dbDescribeComponents(GB_NS::dbFieldDescriptor*) { \
        return &fields; \
    } \
    static GB_NS::dbTableDescriptor dbDescriptor

/**
 * Register table descriptor and assign it to specified database
 */
#if (defined(_MSC_VER) && _MSC_VER+0 < 1200) || defined(__MWERKS__)
    #if defined(_MSC_VER)
        #define GET_TABLE_DESC_PREFIX
    #else
        #define GET_TABLE_DESC_PREFIX template<>
    #endif
    #define GET_TABLE_DESC_PARAM(T) <T>(T*)
#else
    #define GET_TABLE_DESC_PREFIX
    #define GET_TABLE_DESC_PARAM(T) (T*)
#endif

#if defined(_MSC_VER) && _MSC_VER+0 <= 1300
    #define TABLE_DESC_PREFIX 
#else
    #define TABLE_DESC_PREFIX template<>
#endif

#define REGISTER_IN(table, database) \
    GET_TABLE_DESC_PREFIX GB_NS::dbTableDescriptor* dbGetTableDescriptor GET_TABLE_DESC_PARAM(table) \
      { return &table::dbDescriptor; }              \
    static GB_NS::dbFieldDescriptor* dbDescribeComponentsOf##table() \
     { return ((table*)0)->dbDescribeComponents(NULL); }     \
    GB_NS::dbTableDescriptor table::dbDescriptor(_T(#table), database, sizeof(table), \
                                                 &dbDescribeComponentsOf##table)

#define REGISTER_TEMPLATE_IN(table, database) \
    GET_TABLE_DESC_PREFIX GB_NS::dbTableDescriptor* dbGetTableDescriptor GET_TABLE_DESC_PARAM(table) \
      { return &table::dbDescriptor; }              \
    static GB_NS::dbFieldDescriptor* dbDescribeComponentsOf##table() \
     { return ((table*)0)->dbDescribeComponents(NULL); }     \
    TABLE_DESC_PREFIX GB_NS::dbTableDescriptor table::dbDescriptor(_T(#table), database, sizeof(table), \
                                                            &dbDescribeComponentsOf##table)

#define REGISTER_IN_NS(ns, table, database)                              \
    GET_TABLE_DESC_PREFIX GB_NS::dbTableDescriptor* dbGetTableDescriptor GET_TABLE_DESC_PARAM(ns::table) \
    { return &ns::table::dbDescriptor; }                                \
    static GB_NS::dbFieldDescriptor* dbDescribeComponentsOf##ns##__##table() \
    { return ((ns::table*)0)->dbDescribeComponents(NULL); }             \
    GB_NS::dbTableDescriptor ns::table::dbDescriptor(_T(ns##__##table), database, sizeof(ns::table), \
                                                     &dbDescribeComponentsOf##ns##__##table)

#define REGISTER_TEMPLATE_IN_NS(ns, table, database)                              \
    GET_TABLE_DESC_PREFIX GB_NS::dbTableDescriptor* dbGetTableDescriptor GET_TABLE_DESC_PARAM(ns::table) \
    { return &ns::table::dbDescriptor; }                                \
    static GB_NS::dbFieldDescriptor* dbDescribeComponentsOf##ns##__##table() \
    { return ((ns::table*)0)->dbDescribeComponents(NULL); }             \
    TABLE_DESC_PREFIX GB_NS::dbTableDescriptor ns::table::dbDescriptor(_T(ns##__##table), database, sizeof(ns::table), \
                                                                &dbDescribeComponentsOf##ns##__##table)


/**
 * Register table descriptor. It will be assigned to the database when database will be 
 * opened
 */
#define REGISTER(table) REGISTER_IN(table, NULL)
#define REGISTER_TEMPLATE(table) REGISTER_TEMPLATE_IN(table, NULL)
#define REGISTER_NS(ns, table) REGISTER_IN_NS(ns, table, NULL)
#define REGISTER_TEMPLATE_NS(ns, table) REGISTER_TEMPLATE_IN_NS(ns, table, NULL)

/**
 * Register database and mark it as unsigned. Programmer should explicitly
 * specify database in all operations.
 */
#define DETACHED_TABLE ((GB_NS::dbDatabase*)-1)
#define REGISTER_UNASSIGNED(table) REGISTER_IN(table, DETACHED_TABLE)
#define REGISTER_TEMPLATE_UNASSIGNED(table) REGISTER_TEMPLATE_IN(table, DETACHED_TABLE)
#define REGISTER_UNASSIGNED_NS(ns, table) REGISTER_IN_NS(ns, table, DETACHED_TABLE)
#define REGISTER_TEMPLATE_UNASSIGNED_NS(ns, table) REGISTER_TEMPLATE_IN_NS(ns, table, DETACHED_TABLE)


class dbDatabase;
class dbSession;
class dbAnyArray;
class dbTableDescriptor;
class dbAnyMethodTrampoline;
class dbTable;

/**
 * Descriptor of table field
 */
class GIGABASE_DLL_ENTRY dbFieldDescriptor {
  public:
    /**
     * Next file within scope
     */
    dbFieldDescriptor* next;

    /**
     * Previous field within scope
     */
    dbFieldDescriptor* prev;

    /**
     * Next field in the list of all fields in the table
     */
    dbFieldDescriptor* nextField;

    /**
     * Next field in the list of all hashed fields in the table
     */
    dbFieldDescriptor* nextHashedField;

    /**
     * Next field in the list of all indexed fields in the table
     */
    dbFieldDescriptor* nextIndexedField;

    /**
     * Next field in the list of all relation fields in the table
     */
    dbFieldDescriptor* nextInverseField;

    /**
     * Column number
     */
    int                fieldNo;
    
    /**
     * Name of the field
     */
    char_t*            name;

    /**
     * Compound name of field, for example "coord.x"
     */
    char_t*            longName;

    /**
     * Name of referenced table (for reference fields only)
     */
    char_t*            refTableName;

    /**
     * Referenced table (for reference fields only)
     */
    dbTableDescriptor* refTable;

    /**
     * Definition of the table to which this field belongs
     */
    dbTableDescriptor* defTable;

    /**
     * Inverse reference (for reference fields only)
     */
    dbFieldDescriptor* inverseRef;

    /**
     * Inverse reference name (for reference fields only)
     */
    char_t*            inverseRefName;

    /**
     * Type of the field in the database (dbField::FieldTypes)
     */
    int                type;

    /**
     * Type of the field in application
     */
    int                appType;

    /**
     * Type of field index (bit combination of constants defined in dbIndexType)
     */
    int                indexType;

    /**
     * Default value of the field (in case an existing scheme is extended)
     */
    union {		
        bool Bool;		
        int1 Int1;		int2 Int2;		int4 Int4;		db_int8 Db_Int8;
        real4 Real4;	real8 Real8;
    } defaultValue;
    
    /** 
     * Offset to the field in database
     */
    int                dbsOffs;

    /** 
     * Offset to the field in application
     */    
    int                appOffs;

    /**
     * Subcomponents of the field (for structures and arrays)
     */
    dbFieldDescriptor* components;

    /**
     * Hash table (currently not used)
     */
    oid_t              hashTable;

    /**
     * B-Tree (for fields which are indexed by means of T-Ttree)
     */
    oid_t              bTree;

    /**
     * Size of the record in database
     */
    size_t             dbsSize;
    
    /**
     * Size of the object in application
     */
    size_t             appSize;

    /**
     * Alignment of the field (for structures it is equal to the maximum required alignment 
     * of it's components
     */
    size_t             alignment;

    /**
     * Comparator for user defined types
     */
    dbUDTComparator    comparator;

    /**
     * Attributes of the field
     */
    enum FieldAttributes {
        ComponentOfArray   = 0x01,
        HasArrayComponents = 0x02,
        OneToOneMapping    = 0x04,
        Updated            = 0x08,
        HasArrayOfArrayComponents = 0x10
    };
    int                attr;

    /**
     * Old type of the field in database (before schema evaluation)
     */
    int                oldDbsType;
    /**
     * Old offset of the field in database (before schema evaluation)
     */
    int                oldDbsOffs;
    /**
     * Old size of the field in database (before schema evaluation)
     */
    int                oldDbsSize;


    /**
     * Trampoline used to invoke class method from SubSQL (for method components only)
     */
    dbAnyMethodTrampoline* method;

    /**
     * Allocator of array components
     */
    void (*arrayAllocator)(dbAnyArray* array, void* data, size_t length);

    /**
     * Calculate record size in the database.
     * This method performs interation through all components in one scope
     * and recursively invokes itself for structure and array components. 
     * First time this method is invoked by table descriptor with <code>offs</code>
     * equal to size of fixed part of the record.
     * @param base address of the application object 
     * @param offs offset of the end of varying part of the record
     * @return size of the record
     */
    size_t calculateRecordSize(byte* base, size_t offs);

    /**
     * Calculate record size after reformatting record according
     * to the new definition of the application class.
     * This method performs interation thtough all components in one scope
     * and recursively invoke itself for structure and array components. 
     * @param base address of the application object 
     * @param offs offset of the end of varying part of the record
     * @return size of the record
     */
    size_t calculateNewRecordSize(byte* base, size_t offs);
    
    /**
     * Convert of the feild to new format.
     * This method is recursively invoked for array and structure components.     
     * @param dst destination for converted field
     * @param src original field
     * @param offs offset of varying part
     * @param offs offset of the end of varying part of the record
     * @return size of the record
     */
    size_t convertRecord(byte* dst, byte* src, size_t offs);

    /**
     * Size of the record without one field. This method is used to implement 
     * automatically updated inverse references.
     * This method performs interation thtough all components in one scope
     * and recursively invoke itself for structure and array components.      
     * @param field list of the fields in one scope
     * @param base pointer inside database
     * @param size [in/out] size of the record
     * @return offset of last field
     */
    int    sizeWithoutOneField(dbFieldDescriptor* field,
                               byte* base, size_t& size);

    /**
     * Recursively copy record to new location except one field. This method
     * is used for updating inverse references.
     * @param field list of the fields in one scope
     * @param dst destination where record should be copied
     * @param src source of the copy
     * @param offs offset to the end of varying part
     * @return size of the record
     */
    size_t copyRecordExceptOneField(dbFieldDescriptor* field,
                                    byte* dst, byte* src, size_t offs);

    
    enum StoreMode {
        Insert,
        Update,
        Import
    };

    /**
     * Store record fields in the databases
     * This method performs interation thtough all components in one scope
     * and recursively invoke itself for structure and array components.      
     * @param dst place in the database where record should be stored
     * @param src pointer to the application object
     * @param offs offset to the end of varying part
     * @param mode Insert, Update or Import mode
     * @return size of the record
     */     
    size_t storeRecordFields(byte* dst, byte* src, size_t offs, StoreMode mode);

    /**
     * Mask updated fields.
     * This method performs interation thtough all components in one scope
     * and recursively invoke itself for structure and array components.      
     * @param dst old image of the record in the database
     * @param src updated application object
     */
    void markUpdatedFields(byte* dst, byte* src);

    /**
     * Mask updated fields.
     * This method performs interation thtough all components in one scope
     * and recursively invoke itself for structure and array components.      
     * @param dst old image of the record in the database
     * @param src new image of the record in the database
     */
    void markUpdatedFields2(byte* dst, byte* src);

    /**
     * Fetch record from the database
     * This method performs interation thtough all components in one scope
     * and recursively invoke itself for structure and array components.      
     * @param dst pointer to the application object into which record is extract
     * @param src image of the object in the database
     */
     void fetchRecordFields(byte* dst, byte* src);

    /**
     * Find component with specified symbol name (for structures only)
     * @param name component symbol name
     * @return descriptor of the field or <code>NULL</code> if not found
     */
    dbFieldDescriptor* findSymbol(const char_t* name);

    /**
     * Find component with specified name (for structures only)
     * @param name component name
     * @return descriptor of the field or <code>NULL</code> if not found
     */
    dbFieldDescriptor* find(const char_t* name);

    /**
     * Get first component of the field (for structures only)
     * @return first component of the structure
     */
    dbFieldDescriptor* getFirstComponent() { 
        return components;
    }

    /**
     * Get next component within the scope
     * @return next component within the scope
     */
    dbFieldDescriptor* getNextComponent(dbFieldDescriptor* field) { 
        if (field != NULL) { 
            field = field->next;
            if (field == components) { 
                return NULL;
            }
        }
        return field;
    }

    /** 
     * Redefined ',' operator used to form list of components
     */
    dbFieldDescriptor& operator, (dbFieldDescriptor& field) {
        dbFieldDescriptor* tail = field.prev;
        tail->next = this;
        prev->next = &field;
        field.prev = prev;
        prev = tail;
        return *this;
    }

    void* operator new(size_t size);
    void  operator delete(void* p);

    /**
     * Adjust offsets within application objects for descriptors of base classes.
     */
    dbFieldDescriptor& adjustOffsets(size_t offs);

    /**
     * Field descriptor constructor
     * @param name name of the field
     * @param offs offset of the field
     * @param size size of the field
     * @param indexType type of index used for this field
     * @param inverse name of inverse field
     * @param components comopnents of structure or array
     */
    dbFieldDescriptor(char_t const* name, size_t offs, size_t size, int indexType,
                      char_t const* inverse = NULL,
                      dbFieldDescriptor* components = NULL);

    /**
     * Constructor of dummy field descriptor 
     * @param  name name of the field
     */
    dbFieldDescriptor(char_t const* name);

    /**
     * Field descriptor destructor
     */
    ~dbFieldDescriptor();
};


/**
 * Table descriptor
 */
class GIGABASE_DLL_ENTRY dbTableDescriptor {
    friend class dbCompiler;
    friend class dbDatabase;
    friend class dbReplicatedDatabase;
    friend class dbTable;
    friend class dbAnyCursor;
    friend class dbSubSql;
    friend class dbParallelQueryContext;
    friend class dbServer;
    friend class dbSession;
    friend class dbAnyContainer;
    friend class dbColumnBinding;
    friend class dbFieldDescriptor;
    friend class dbSelection;
    friend class dbTableIterator;
    friend class dbCLI;
  protected:
    /**
     * Chain of all tables in application
     */
    dbTableDescriptor*  next;
    static dbTableDescriptor* chain;
    static dbMutex*           chainMutex;

    /**
     * Chain of all tables associated with database
     */
    dbTableDescriptor*  nextDbTable; // next table in the database

    /**
     * Name of the table
     */
    char_t*             name;

    /**
     * Indetifier of table object in the database
     */
    oid_t               tableId;

    /**
     * List of table columns
     */
    dbFieldDescriptor*  columns;
    
    /**
     * List of hashed fields
     */
    dbFieldDescriptor*  hashedFields;

    /**
     * List of fields indexed by B-Ttree
     */
    dbFieldDescriptor*  indexedFields;

    /**
     * List of related fields (fields, for which inverse references exist)
     */
    dbFieldDescriptor*  inverseFields;

    /**
     * List of all fields
     */
    dbFieldDescriptor*  firstField;

    /**
     * Pointer of next field of the last field (used for list construction)
     */
    dbFieldDescriptor** nextFieldLink;

    /**
     * Attached database
     */
    dbDatabase*         db;
    
    /**
     * Database staticly attached to the table (by means of REGISTER_IN macro)
     */
    bool                fixedDatabase;

    /**
     * Table descriptor is static object created by one of REGISTER macros
     */
    bool                isStatic;
    
    /**
     * When unassigned table descriptor is explicitly assigned to the database, 
     * new clone of descriptor is created and  <code>cloneOf</code> field of this descriptor
     * referes to original table descriptor.
     */
    dbTableDescriptor*  cloneOf;

    /**
     * Size of tghe correspondent application object
     */
    size_t              appSize;

    /**
     * Size of fixed part of the records (without string and array bodies)
     */
    size_t              fixedSize;

    /**
     * Number of fields in the table
     */
    size_t              nFields;

    /**
     * Number of columns in the table
     */
    size_t              nColumns;

    /**
     * Identifer of the first (oldest) row in the table
     */
    oid_t               firstRow;

    /**
     * Identifer of the last (most recently inerted) row in the table
     */
    oid_t               lastRow;
    
    /**
     * Number of the rows in the table
     */
    size_t              nRows;

    /**
     * Autoincremented counter for this table
     */
    int4                autoincrementCount;

    /**
     * Next table with batch inserted records
     */
    dbTableDescriptor*        nextBatch;

    /**
     * If table contains batch inserted records
     */
    bool                      isInBatch;

    /**
     * Selection to hold OID of batch inserted records
     */
    dbSelection               batch;

    /**
     * ID of transaction when this table descriptor was last refreshed
     */
    int                       transactionId;


    /**
     * Table attrbiutes
     */
    int                       attr;

    /**
     * Function returning list of record fields descriptors
     */
    typedef dbFieldDescriptor* (*describeFunc)();
    describeFunc        describeComponentsFunc;

    /**
     * Collision chain of table's hash table
     */
    dbTableDescriptor*  collisionChain;

    /**
     * Calculate total length of all names in table descriptor
     */
    size_t              totalNamesLength();

    /**
     * Recursively set field attributes.
     * @param fieldsList list of record fields
     * @param prefix prefix for the field (in case of structures or arrays 
     * this functions is invoked resursively for components of this structure or
     * or array
     * @param offs - offset in application class
     * @param indexMask index mask for the structore containing the field
     * @param attr attributes of the parent field
     * @return alignment of the field
     * @param dbsAlignment maximal alignment of the fields in the database 
     * @param appAlignment maximal alignment of the fields in the application 
     */
     void calculateFieldsAttributes(dbFieldDescriptor* fieldsList,
                                  char_t const* prefix, int offs,
                                  int indexMask, int& attr,
                                  size_t& dbsAlignment, size_t& appAlignment);

    /**
     * Read table definiton from the database and build fields list
     * @param table databsae table descriptor
     * @param prefix prefix for the field (in case of structures or arrays 
     * @param prefixLen length of the prefix
     * @param attr attributes of the parent field
     * @return pointer to the constructed list
     */
    dbFieldDescriptor* buildFieldsList(dbTable* table, char_t const* prefix,
                                       int prefixLen, int& attr);
    /**
     * Clone table descriptor
     */
    dbTableDescriptor* clone();

  public:
    /**
     * Initial value for autoincrement conunt
     */
    static int initialAutoincrementCount;


    /**
     * Get next table in database
     */
    dbTableDescriptor* getNextTable() { 
        return nextDbTable;
    }

    /**
     * Find field with specified symbol name
     */    
    dbFieldDescriptor* findSymbol(char_t const* name);

    /**
     * Find field with specified name
     */    
    dbFieldDescriptor* find(char_t const* name);

    /**
     * Get first record field
     * @return descriptor of first record field
     */
     dbFieldDescriptor* getFirstField() { 
        return columns;
    }

    /**
     * Get last value of autoincrement counter used for this table
     * @return last value of autoincrement counter used for this table
     */
    int getLastValueOfAutoincrementCount() const { 
        return autoincrementCount;
    }

    /**
     * Get next field
     * @param field current  field
     * @return next field after the current in table fields list
     */
    dbFieldDescriptor* getNextField(dbFieldDescriptor* field) { 
        if (field != NULL) { 
            field = field->next;
            if (field == columns) { 
                return NULL;
            }
        }
        return field;
    }

    /**
     * Get table name.
     */
    char_t* getName() { 
        return name;
    }

    /**
     * Get size of instance of the class in an application
     */
    size_t size() { 
        return appSize;
    }

    /**
     * Check whether table descriptor in the database is the same as
     * table appplication table descriptor
     * @param table database table descriptor
     * @param ignoreIndices ignore presence of indices during comparison
     * @return <code>true</code> if two table descriptors are equal
     */
    bool equal(dbTable* table, bool ignoreIndices = false);

    /**
     * Check whether fprmats of table descriptor in the database 
     * and in application is compatible. This method also prepares 
     * information for performing conversion of record to new format
     * @param table database table descriptor
     * @param confirmDeleteColumns whether deletion of columns in allowed from non empty table
     * @param isEmpty table is empty
     * @return <code>true</code> if no reformatting is needed
     */
    bool match(dbTable* table, bool confirmDeleteColumns, bool preserveExistedIndices, bool isEmpty);

    /**
     * Check consuistency of declared realations (check that referenced table 
     * actually contains declared inverse reference field). 
     * This method also resolve references between table.
     */
    bool checkRelationship();

    /**
     * Get reference to associated database
     * @return database to which this table is assigned
     */
    dbDatabase* getDatabase() { return db; }

    /**
     * Save table descriptor in the database.
     * @param table place where to store table descriptor
     */
    void storeInDatabase(dbTable* table);

    /**
     * Set fields flags.  This method is called after loading table descriptor
     * from database.
     */
    void setFlags();

    static dbMutex& getChainMutex();
    void link();
    void unlink();

    /**
     * Remove all table descriptors except static ones
     */
    static void cleanup();


    /**
     * Construct table descriptor using information stored in database
     * @param table pointer to database table descriptor
     */
    dbTableDescriptor(dbTable* table);

    /**
     * Constructor of application table descriptor
     * @param tableName name of the table
     * @param db assigned database (may be NULL)
     * @param objSize size of application object
     * @param func function returninglist of field descriptors
     * @param original roiginal table descriptor (for cloned descriptors)
     */
    dbTableDescriptor(char_t const*      tableName, 
                      dbDatabase*        db, 
                      size_t             objSize,
                      describeFunc       func,
                      dbTableDescriptor* original = NULL);

    /**
     * Table descriptor destructor
     */
     ~dbTableDescriptor();
};

/**
 * Header of database array or string
 */
struct dbVarying {
    nat4 size; // number of elements in the array
    int4 offs; // offset from the beginning of the record
};

/**
 * Database record for storing field descriptor
 */
struct dbField {
    enum FieldTypes {
        tpBool,
        tpInt1,
        tpInt2,
        tpInt4,
        tpInt8,
        tpReal4,
        tpReal8,
        tpString,
        tpReference,
        tpArray,
        tpMethodBool,
        tpMethodInt1,
        tpMethodInt2,
        tpMethodInt4,
        tpMethodInt8,
        tpMethodReal4,
        tpMethodReal8,
        tpMethodString,
        tpMethodReference,
        tpStructure,
        tpRawBinary,
        tpStdString,
        tpMfcString,
        tpRectangle,
        tpUnknown
    };

    /**
     * Full name of the field (for example "x.y.z")
     */
    dbVarying name;

    /**
     * Name of referenced table( only for references)
     */
    dbVarying tableName; // only for references: name of referenced table

    /**
     * Name of inverse reference field (only for refereces)
     */
    dbVarying inverse;   // only for relations: name of inverse reference field
    
    /**
     * Field type: one of <code>dbField::FieldTypes</code> constants
     */
#ifdef OLD_FIELD_DESCRIPTOR_FORMAT
    int4      type;
#else 
#if BYTE_ORDER == BIG_ENDIAN
    int4      flags : 24;
    int4      type  : 8;
#else
    int4      type  : 8;
    int4      flags : 24;
#endif
#endif

    /**
     *  Offset of the field in the record
     */
    int4      offset;

    /**
     * Size of the field
     */
    nat4      size;

    /**
     * Hash table for hashed field
     */
    oid_t     hashTable;

    /**
     * B-Tree for field indexed by means of B-Ttree
     */
    oid_t     bTree;
};


/**
 * Header of any database record
 */
class dbRecord {
  public:
    /**
     * Size of the record (including header
     */
    nat4   size;

    /**
     * Next record in the table (0 if it is last record)
     */
    oid_t  next;

    /**
     * Previous record in the table (0 if it is first record)
     */
    oid_t  prev;
};


/**
 * Database recod for storing table descriptor
 */
class dbTable : public dbRecord {
  public:
    /**
     * Name of the table
     */
    dbVarying name;
    
    /**
     * Array with field descriptors
     */
    dbVarying fields;

    /**
     * Size of fixed part of the record (without string and arrays bodies)
     */
    nat4      fixedSize;

    /**
     * Number of rows in the table
     */
    nat4      nRows;

    /**
     * Number of columns in the table
     */
    nat4      nColumns;
    
    /**
     * Identifier of first row in the table
     */
    oid_t     firstRow;

    /**
     * Identifier of last row in the table
     */
    oid_t     lastRow;
#ifdef AUTOINCREMENT_SUPPORT
    /**
     * Autoincremented counter
     */
    nat4      count;
#endif
};

inline dbFieldDescriptor* dbDescribeRawField(dbFieldDescriptor* fd, dbUDTComparator comparator)
{
    fd->type = fd->appType = dbField::tpRawBinary;
    fd->alignment = 1;
    fd->comparator = comparator;
    return fd;
}

inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, int1&, int1 defaultValue = 0)
{
    fd->type = fd->appType = dbField::tpInt1;
	fd->defaultValue.Int1 = defaultValue;
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, int2&, int2 defaultValue = 0)
{
    fd->type = fd->appType = dbField::tpInt2;
	fd->defaultValue.Int2 = defaultValue;
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, int4&, int4 defaultValue = 0)
{
    fd->type = fd->appType = dbField::tpInt4;
	fd->defaultValue.Int4 = defaultValue;
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, db_int8&, db_int8 defaultValue = 0)
{
    fd->type = fd->appType = dbField::tpInt8;
	fd->defaultValue.Db_Int8 = defaultValue;
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, nat1&, nat1 defaultValue = 0)
{
    fd->type = fd->appType = dbField::tpInt1;
	fd->defaultValue.Int1 = defaultValue;
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, nat2&, nat2 defaultValue = 0)
{
    fd->type = fd->appType = dbField::tpInt2;
	fd->defaultValue.Int2 = defaultValue;
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, nat4&, nat4 defaultValue = 0)
{
	fd->type = fd->appType = dbField::tpInt4;
	fd->defaultValue.Int4 = defaultValue;
	return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, nat8&, nat8 defaultValue = 0)
{
    fd->type = fd->appType = dbField::tpInt8;
	fd->defaultValue.Db_Int8 = defaultValue;
    return fd;
}

#if SIZEOF_LONG != 8 
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, long&, long defaultValue = 0)
{
    fd->type = fd->appType = sizeof(long) == 4 ? dbField::tpInt4 : dbField::tpInt8;
    if (sizeof(long) == 4) fd->defaultValue.Int4 = defaultValue;
    else fd->defaultValue.Db_Int8 = defaultValue;
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, unsigned long&, unsigned long defaultValue = 0)
{
    fd->type = fd->appType = sizeof(long) == 4 ? dbField::tpInt4 : dbField::tpInt8;
    if (sizeof(long) == 4) fd->defaultValue.Int4 = defaultValue;
    else fd->defaultValue.Db_Int8 = defaultValue;
    return fd;
}
#endif

inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, bool&, bool defaultValue = false)
{
    fd->type = fd->appType = dbField::tpBool;
	fd->defaultValue.Bool = defaultValue;
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, real4&, real4 defaultValue = 0.0)
{
    fd->type = fd->appType = dbField::tpReal4;
	fd->defaultValue.Real4 = defaultValue;
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, real8&, real8 defaultValue = 0.0)
{
    fd->type = fd->appType = dbField::tpReal8;
	fd->defaultValue.Real8 = defaultValue;
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, rectangle&)
{
    fd->type = fd->appType = dbField::tpRectangle;
    fd->alignment = sizeof(coord_t);
    return fd;
}

#ifdef USE_STD_STRING
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, STD_STRING&)
{
    fd->type = dbField::tpString;
    fd->appType = dbField::tpStdString;
    fd->dbsSize = sizeof(dbVarying);
    fd->alignment = 4;
    fd->components = new dbFieldDescriptor(STRLITERAL("[]"));
    fd->components->type = fd->components->appType = dbField::tpInt1 + (sizeof(char_t) - 1);
    fd->components->dbsSize = fd->components->appSize = fd->components->alignment = sizeof(char_t); 
    return fd;
}
#endif
#ifdef USE_MFC_STRING
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, MFC_STRING&)
{
    fd->type = dbField::tpString;
    fd->appType = dbField::tpMfcString;
    fd->dbsSize = sizeof(dbVarying);
    fd->alignment = 4;
    fd->components = new dbFieldDescriptor(STRLITERAL("[]"));
    fd->components->type = fd->components->appType = dbField::tpInt1 + (sizeof(char_t) - 1);
    fd->components->dbsSize = fd->components->appSize = fd->components->alignment = sizeof(char_t); 
    return fd;
}
#endif

inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, char_t const*&)
{
    fd->type = fd->appType = dbField::tpString;
    fd->dbsSize = sizeof(dbVarying);
    fd->alignment = 4;
    fd->components = new dbFieldDescriptor(STRLITERAL("[]"));
    fd->components->type = fd->components->appType = dbField::tpInt1 + (sizeof(char_t) - 1);
    fd->components->dbsSize = fd->components->appSize = fd->components->alignment = sizeof(char_t); 
    return fd;
}
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, char_t*&)
{
    fd->type = fd->appType = dbField::tpString;
    fd->dbsSize = sizeof(dbVarying);
    fd->alignment = 4;
    fd->components = new dbFieldDescriptor(STRLITERAL("[]"));
    fd->components->type = fd->components->appType = dbField::tpInt1 + (sizeof(char_t) - 1);
    fd->components->dbsSize = fd->components->appSize = fd->components->alignment = sizeof(char_t);
    return fd;
}


template<class T>
inline dbFieldDescriptor* dbDescribeField(dbFieldDescriptor* fd, T& x)
{
    fd->type = fd->appType = dbField::tpStructure;
    fd->components = x.dbDescribeComponents(fd);	
    return fd;
}


/** 
 * Trampoline for invocation of methods from SubSQL
 */
class GIGABASE_DLL_ENTRY dbAnyMethodTrampoline {
  public:
    dbFieldDescriptor* cls;

    /**
     * Invoke method
     * @param data pointer to the record insode database
     * @param result pointer to place result in 
     */
    virtual void invoke(byte* data, void* result) = 0;

    /**
     * Get optimize trampoline. Optimized trampoline can be used for records
     * which format in the database is the same as in application. In this case
     * there is no need to fetch record and pointer insode dataabse can be used intead
     * @return optimized nethod trampoline
     */
    virtual dbAnyMethodTrampoline* optimize() = 0;

    /**
     * Method tramopile constructor
     * @param fd method descriptor
     */
    dbAnyMethodTrampoline(dbFieldDescriptor* fd) { cls = fd; }
    
    /**
     * Trampoline desctructor
     */
    virtual~dbAnyMethodTrampoline();

    void* operator new(size_t size);
    void  operator delete(void* p);
};

#if defined(__APPLE__) || defined(__VACPP_MULTI__) || defined(__IBMCPP__) || defined(__HP_aCC) || (defined(__SUNPRO_CC) && __SUNPRO_CC >= 0x510 && __SUNPRO_CC_COMPAT == 5)
/**
 * Template for method trampoline implementation
 */
template<class T, class R>
class dbMethodTrampoline : public dbAnyMethodTrampoline {
  public:
    typedef R (T::*mfunc)();

    mfunc              method;
    dbFieldDescriptor* cls;
    bool               optimized;

    void invoke(byte* data, void* result) {
        if (optimized) { 
            *(R*)result = (((T*)(data + this->cls->dbsOffs))->*method)();
        } else { 
            T rec;
            this->cls->components->fetchRecordFields((byte*)&rec, data);
            *(R*)result = (rec.*method)();
        }
    }
    dbAnyMethodTrampoline* optimize() { 
        optimized = true;
        return this;
    }

    dbMethodTrampoline(dbFieldDescriptor* fd, mfunc f)
    : dbAnyMethodTrampoline(fd)
    {
        this->method = f;
        this->cls = fd;
        this->optimized = false;
    }
};

#else

/**
 * Template for method trampoline implementation
 */
template<class T, class R>
class dbMethodTrampoline : public dbAnyMethodTrampoline {
  public:
    typedef R (T::*mfunc)();
    mfunc method;

    void invoke(byte* data, void* result) {
        T rec;
        this->cls->components->fetchRecordFields((byte*)&rec, data);
        *(R*)result = (rec.*method)();
    }
    dbAnyMethodTrampoline* optimize();

    dbMethodTrampoline(dbFieldDescriptor* fd, mfunc f)
    : dbAnyMethodTrampoline(fd), method(f) {}
};

template<class T, class R>
class dbMethodFastTrampoline : public dbAnyMethodTrampoline {
    typedef R (T::*mfunc)();
    mfunc method;
  public:
    dbAnyMethodTrampoline* optimize() { 
        return this;
    }
    void invoke(byte* data, void* result) {
        *(R*)result = (((T*)(data + this->cls->dbsOffs))->*method)();
    }
    dbMethodFastTrampoline(dbMethodTrampoline<T,R>* mt) 
    : dbAnyMethodTrampoline(mt->cls), method(mt->method) {
        delete mt;
    }
};

template<class T, class R>
inline dbAnyMethodTrampoline* dbMethodTrampoline<T,R>::optimize() {
    return new dbMethodFastTrampoline<T,R>(this);
}

#endif
template<class T, class R>
inline dbFieldDescriptor* dbDescribeMethod(dbFieldDescriptor* fd, R (T::*p)())
{
    R ret;
    dbDescribeField(fd, ret);
    assert(fd->type <= dbField::tpReference);
    fd->appType = fd->type += dbField::tpMethodBool;
    fd->method = new dbMethodTrampoline<T,R>(fd, p);
    return fd;
}

END_GIGABASE_NAMESPACE

#endif



/*
 * CppSQLite
 * Developed by Rob Groves <rob.groves@btinternet.com>
 * Maintained by NeoSmart Technologies <http://neosmart.net/>
 * See LICENSE file for copyright and license info
 */

#include "CppSQLite3.h"
#include <cstdlib>
#include <cstring>
#include <utility>

// Named constant for passing to CppSQLite3Exception when passing it a string
// that cannot be deleted.
static const bool DONT_DELETE_MSG = false;

// Error message used when throwing CppSQLite3Exception when allocations fail.
static const char *const ALLOCATION_ERROR_MESSAGE = "Cannot allocate memory";

////////////////////////////////////////////////////////////////////////////////
// Prototypes for SQLite functions not included in SQLite DLL, but copied below
// from SQLite encode.c
////////////////////////////////////////////////////////////////////////////////
int sqlite3_encode_binary(const unsigned char *in, int n, unsigned char *out);
int sqlite3_decode_binary(const unsigned char *in, unsigned char *out);

////////////////////////////////////////////////////////////////////////////////

namespace detail {

SQLite3Memory::SQLite3Memory() : mpBuf(nullptr), mnBufferLen(0) {}

SQLite3Memory::SQLite3Memory(size_t nBufferLen)
    : mpBuf(sqlite3_malloc(static_cast<int>(nBufferLen))),
      mnBufferLen(nBufferLen) {
    if (!mpBuf && mnBufferLen > 0) {
        throw CppSQLite3Exception(CPPSQLITE_ERROR, ALLOCATION_ERROR_MESSAGE,
                                  DONT_DELETE_MSG);
    }
}

SQLite3Memory::SQLite3Memory(const char *szFormat, va_list list)
    : mpBuf(sqlite3_vmprintf(szFormat, list)), mnBufferLen(0) {
    if (!mpBuf) {
        throw CppSQLite3Exception(CPPSQLITE_ERROR, ALLOCATION_ERROR_MESSAGE,
                                  DONT_DELETE_MSG);
    }
    mnBufferLen = std::strlen(static_cast<char const *>(mpBuf)) + 1;
}

SQLite3Memory::~SQLite3Memory() { clear(); }

SQLite3Memory::SQLite3Memory(SQLite3Memory const &other)
    : mpBuf(sqlite3_malloc(static_cast<int>(other.mnBufferLen))),
      mnBufferLen(other.mnBufferLen) {
    if (!mpBuf && mnBufferLen > 0) {
        throw CppSQLite3Exception(CPPSQLITE_ERROR, ALLOCATION_ERROR_MESSAGE,
                                  DONT_DELETE_MSG);
    }
    if (mnBufferLen > 0) {
        std::memcpy(mpBuf, other.mpBuf, mnBufferLen);
    }
}

SQLite3Memory &SQLite3Memory::operator=(SQLite3Memory const &lhs) {
    SQLite3Memory tmp(lhs);
    swap(tmp);
    return *this;
}

SQLite3Memory::SQLite3Memory(SQLite3Memory &&other)
    : mpBuf(other.mpBuf), mnBufferLen(other.mnBufferLen) {
    other.mnBufferLen = 0;
    other.mpBuf = nullptr;
}

SQLite3Memory &SQLite3Memory::operator=(SQLite3Memory &&lhs) {
    swap(lhs);
    return *this;
}

void SQLite3Memory::swap(SQLite3Memory &other) {
    std::swap(mnBufferLen, other.mnBufferLen);
    std::swap(mpBuf, other.mpBuf);
}

void SQLite3Memory::clear() {
    sqlite3_free(mpBuf);
    mpBuf = nullptr;
    mnBufferLen = 0;
}

} // namespace detail

////////////////////////////////////////////////////////////////////////////////

CppSQLite3Exception::CppSQLite3Exception(const int nErrCode,
                                         const char *szErrMess,
                                         bool bDeleteMsg /*=true*/)
    : mnErrCode(nErrCode) {
    mpszErrMess = sqlite3_mprintf("%s[%d]: %s", errorCodeAsString(nErrCode),
                                  nErrCode, szErrMess ? szErrMess : "");

    if (bDeleteMsg && szErrMess) {
        sqlite3_free(static_cast<void *>(const_cast<char *>(szErrMess)));
    }
}

CppSQLite3Exception::CppSQLite3Exception(const CppSQLite3Exception &e)
    : mnErrCode(e.mnErrCode) {
    mpszErrMess = nullptr;
    if (e.mpszErrMess) {
        mpszErrMess = sqlite3_mprintf("%s", e.mpszErrMess);
    }
}

const char *CppSQLite3Exception::errorCodeAsString(int nErrCode) {
    switch (nErrCode) {
    case SQLITE_OK:
        return "SQLITE_OK";
    case SQLITE_ERROR:
        return "SQLITE_ERROR";
    case SQLITE_INTERNAL:
        return "SQLITE_INTERNAL";
    case SQLITE_PERM:
        return "SQLITE_PERM";
    case SQLITE_ABORT:
        return "SQLITE_ABORT";
    case SQLITE_BUSY:
        return "SQLITE_BUSY";
    case SQLITE_LOCKED:
        return "SQLITE_LOCKED";
    case SQLITE_NOMEM:
        return "SQLITE_NOMEM";
    case SQLITE_READONLY:
        return "SQLITE_READONLY";
    case SQLITE_INTERRUPT:
        return "SQLITE_INTERRUPT";
    case SQLITE_IOERR:
        return "SQLITE_IOERR";
    case SQLITE_CORRUPT:
        return "SQLITE_CORRUPT";
    case SQLITE_NOTFOUND:
        return "SQLITE_NOTFOUND";
    case SQLITE_FULL:
        return "SQLITE_FULL";
    case SQLITE_CANTOPEN:
        return "SQLITE_CANTOPEN";
    case SQLITE_PROTOCOL:
        return "SQLITE_PROTOCOL";
    case SQLITE_EMPTY:
        return "SQLITE_EMPTY";
    case SQLITE_SCHEMA:
        return "SQLITE_SCHEMA";
    case SQLITE_TOOBIG:
        return "SQLITE_TOOBIG";
    case SQLITE_CONSTRAINT:
        return "SQLITE_CONSTRAINT";
    case SQLITE_MISMATCH:
        return "SQLITE_MISMATCH";
    case SQLITE_MISUSE:
        return "SQLITE_MISUSE";
    case SQLITE_NOLFS:
        return "SQLITE_NOLFS";
    case SQLITE_AUTH:
        return "SQLITE_AUTH";
    case SQLITE_FORMAT:
        return "SQLITE_FORMAT";
    case SQLITE_RANGE:
        return "SQLITE_RANGE";
    case SQLITE_ROW:
        return "SQLITE_ROW";
    case SQLITE_DONE:
        return "SQLITE_DONE";
    case CPPSQLITE_ERROR:
        return "CPPSQLITE_ERROR";
    default:
        return "UNKNOWN_ERROR";
    }
}

CppSQLite3Exception::~CppSQLite3Exception() {
    if (mpszErrMess) {
        sqlite3_free(mpszErrMess);
        mpszErrMess = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////

void CppSQLite3Buffer::clear() { mBuf.clear(); }

const char *CppSQLite3Buffer::format(const char *szFormat, ...) {
    va_list va;
    detail::SQLite3Memory tmpBuf;
    try {
        va_start(va, szFormat);
        tmpBuf = detail::SQLite3Memory(szFormat, va);
        va_end(va);
    } catch (CppSQLite3Exception &) {
        va_end(va);
        throw;
    }
    mBuf = std::move(tmpBuf);
    return static_cast<const char *>(mBuf.getBuffer());
}

////////////////////////////////////////////////////////////////////////////////

CppSQLite3Binary::CppSQLite3Binary()
    : mpBuf(nullptr), mnBinaryLen(0), mnBufferLen(0), mnEncodedLen(0),
      mbEncoded(false) {}

CppSQLite3Binary::~CppSQLite3Binary() { clear(); }

void CppSQLite3Binary::setBinary(const unsigned char *pBuf, size_t nLen) {
    mpBuf = allocBuffer(nLen);
    memcpy(mpBuf, pBuf, nLen);
}

void CppSQLite3Binary::setEncoded(const unsigned char *pBuf) {
    clear();

    mnEncodedLen = strlen(reinterpret_cast<const char *>(pBuf));
    mnBufferLen = mnEncodedLen + 1; // Allow for NULL terminator

    mpBuf = reinterpret_cast<unsigned char *>(malloc(mnBufferLen));

    if (!mpBuf) {
        throw CppSQLite3Exception(CPPSQLITE_ERROR, ALLOCATION_ERROR_MESSAGE,
                                  DONT_DELETE_MSG);
    }

    memcpy(mpBuf, pBuf, mnBufferLen);
    mbEncoded = true;
}

const unsigned char *CppSQLite3Binary::getEncoded() {
    if (!mbEncoded) {
        unsigned char *ptmp =
            reinterpret_cast<unsigned char *>(malloc(mnBinaryLen));
        memcpy(ptmp, mpBuf, mnBinaryLen);
        mnEncodedLen = static_cast<size_t>(
            sqlite3_encode_binary(ptmp, static_cast<int>(mnBinaryLen), mpBuf));
        free(ptmp);
        mbEncoded = true;
    }

    return mpBuf;
}

const unsigned char *CppSQLite3Binary::getBinary() {
    if (mbEncoded) {
        // in/out buffers can be the same
        int result = sqlite3_decode_binary(mpBuf, mpBuf);

        if (result == -1) {
            throw CppSQLite3Exception(CPPSQLITE_ERROR, "Cannot decode binary",
                                      DONT_DELETE_MSG);
        }
        mnBinaryLen = static_cast<size_t>(result);
        mbEncoded = false;
    }

    return mpBuf;
}

size_t CppSQLite3Binary::getBinaryLength() {
    getBinary();
    return mnBinaryLen;
}

unsigned char *CppSQLite3Binary::allocBuffer(size_t nLen) {
    clear();

    // Allow extra space for encoded binary as per comments in
    // SQLite encode.c See bottom of this file for implementation
    // of SQLite functions use 3 instead of 2 just to be sure ;-)
    mnBinaryLen = nLen;
    mnBufferLen = 3 + (257 * nLen) / 254;

    mpBuf = static_cast<unsigned char *>(malloc(mnBufferLen));

    if (!mpBuf) {
        throw CppSQLite3Exception(CPPSQLITE_ERROR, ALLOCATION_ERROR_MESSAGE,
                                  DONT_DELETE_MSG);
    }

    mbEncoded = false;

    return mpBuf;
}

void CppSQLite3Binary::clear() {
    if (mpBuf) {
        mnBinaryLen = 0;
        mnBufferLen = 0;
        free(mpBuf);
        mpBuf = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////

CppSQLite3Query::CppSQLite3Query() {
    mpVM = nullptr;
    mbEof = true;
    mnCols = 0;
    mbOwnVM = false;
}

CppSQLite3Query::CppSQLite3Query(const CppSQLite3Query &rQuery) {
    mpVM = rQuery.mpVM;
    // Only one object can own the VM
    const_cast<CppSQLite3Query &>(rQuery).mpVM = nullptr;
    mbEof = rQuery.mbEof;
    mnCols = rQuery.mnCols;
    mbOwnVM = rQuery.mbOwnVM;
}

CppSQLite3Query::CppSQLite3Query(sqlite3 *pDB, sqlite3_stmt *pVM, bool bEof,
                                 bool bOwnVM /*=true*/) {
    mpDB = pDB;
    mpVM = pVM;
    mbEof = bEof;
    mnCols = sqlite3_column_count(mpVM);
    mbOwnVM = bOwnVM;
}

CppSQLite3Query::~CppSQLite3Query() {
    try {
        finalize();
    } catch (...) {
    }
}

CppSQLite3Query &CppSQLite3Query::operator=(const CppSQLite3Query &rQuery) {
    try {
        finalize();
    } catch (...) {
    }
    mpVM = rQuery.mpVM;
    // Only one object can own the VM
    const_cast<CppSQLite3Query &>(rQuery).mpVM = nullptr;
    mbEof = rQuery.mbEof;
    mnCols = rQuery.mnCols;
    mbOwnVM = rQuery.mbOwnVM;
    return *this;
}

int CppSQLite3Query::numFields() const {
    checkVM();
    return mnCols;
}

const char *CppSQLite3Query::fieldValue(int nField) const {
    checkVM();

    if (nField < 0 || nField >= mnCols) {
        throw CppSQLite3Exception(
            CPPSQLITE_ERROR, "Invalid field index requested", DONT_DELETE_MSG);
    }

    return reinterpret_cast<const char *>(sqlite3_column_text(mpVM, nField));
}

const char *CppSQLite3Query::fieldValue(const char *szField) const {
    int nField = fieldIndex(szField);
    return reinterpret_cast<const char *>(sqlite3_column_text(mpVM, nField));
}

int CppSQLite3Query::getIntField(int nField, int nNullValue /*=0*/) const {
    if (fieldDataType(nField) == SQLITE_NULL) {
        return nNullValue;
    } else {
        return sqlite3_column_int(mpVM, nField);
    }
}

int CppSQLite3Query::getIntField(const char *szField,
                                 int nNullValue /*=0*/) const {
    int nField = fieldIndex(szField);
    return getIntField(nField, nNullValue);
}

long long CppSQLite3Query::getInt64Field(int nField,
                                         long long nNullValue /*=0*/) const {
    if (fieldDataType(nField) == SQLITE_NULL) {
        return nNullValue;
    } else {
        return sqlite3_column_int64(mpVM, nField);
    }
}

long long CppSQLite3Query::getInt64Field(const char *szField,
                                         long long nNullValue /*=0*/) const {
    int nField = fieldIndex(szField);
    return getInt64Field(nField, nNullValue);
}

float CppSQLite3Query::getFloatField(int nField,
                                     float fNullValue /*=0.0f*/) const {
    if (fieldDataType(nField) == SQLITE_NULL) {
        return fNullValue;
    } else {
        return static_cast<float>(sqlite3_column_double(mpVM, nField));
    }
}

float CppSQLite3Query::getFloatField(const char *szField,
                                     float fNullValue /*=0.0f*/) const {
    int nField = fieldIndex(szField);
    return getFloatField(nField, fNullValue);
}

double CppSQLite3Query::getDoubleField(int nField,
                                       double dNullValue /*=0.0*/) const {
    if (fieldDataType(nField) == SQLITE_NULL) {
        return dNullValue;
    } else {
        return sqlite3_column_double(mpVM, nField);
    }
}

double CppSQLite3Query::getDoubleField(const char *szField,
                                       double dNullValue /*=0.0*/) const {
    int nField = fieldIndex(szField);
    return getDoubleField(nField, dNullValue);
}

const char *
CppSQLite3Query::getStringField(int nField,
                                const char *szNullValue /*=""*/) const {
    if (fieldDataType(nField) == SQLITE_NULL) {
        return szNullValue;
    } else {
        return reinterpret_cast<const char *>(
            sqlite3_column_text(mpVM, nField));
    }
}

const char *
CppSQLite3Query::getStringField(const char *szField,
                                const char *szNullValue /*=""*/) const {
    int nField = fieldIndex(szField);
    return getStringField(nField, szNullValue);
}

const unsigned char *CppSQLite3Query::getBlobField(int nField,
                                                   size_t &nLen) const {
    checkVM();

    if (nField < 0 || nField >= mnCols) {
        throw CppSQLite3Exception(
            CPPSQLITE_ERROR, "Invalid field index requested", DONT_DELETE_MSG);
    }

    nLen = static_cast<size_t>(sqlite3_column_bytes(mpVM, nField));
    return reinterpret_cast<const unsigned char *>(
        sqlite3_column_blob(mpVM, nField));
}

const unsigned char *CppSQLite3Query::getBlobField(const char *szField,
                                                   size_t &nLen) const {
    int nField = fieldIndex(szField);
    return getBlobField(nField, nLen);
}

bool CppSQLite3Query::fieldIsNull(int nField) const {
    return (fieldDataType(nField) == SQLITE_NULL);
}

bool CppSQLite3Query::fieldIsNull(const char *szField) const {
    int nField = fieldIndex(szField);
    return (fieldDataType(nField) == SQLITE_NULL);
}

int CppSQLite3Query::fieldIndex(const char *szField) const {
    checkVM();

    if (szField) {
        for (int nField = 0; nField < mnCols; nField++) {
            const char *szTemp = sqlite3_column_name(mpVM, nField);

            if (strcmp(szField, szTemp) == 0) {
                return nField;
            }
        }
    }

    throw CppSQLite3Exception(CPPSQLITE_ERROR, "Invalid field name requested",
                              DONT_DELETE_MSG);
}

const char *CppSQLite3Query::fieldName(int nCol) const {
    checkVM();

    if (nCol < 0 || nCol >= mnCols) {
        throw CppSQLite3Exception(
            CPPSQLITE_ERROR, "Invalid field index requested", DONT_DELETE_MSG);
    }

    return sqlite3_column_name(mpVM, nCol);
}

const char *CppSQLite3Query::fieldDeclType(int nCol) const {
    checkVM();

    if (nCol < 0 || nCol >= mnCols) {
        throw CppSQLite3Exception(
            CPPSQLITE_ERROR, "Invalid field index requested", DONT_DELETE_MSG);
    }

    return sqlite3_column_decltype(mpVM, nCol);
}

int CppSQLite3Query::fieldDataType(int nCol) const {
    checkVM();

    if (nCol < 0 || nCol >= mnCols) {
        throw CppSQLite3Exception(
            CPPSQLITE_ERROR, "Invalid field index requested", DONT_DELETE_MSG);
    }

    return sqlite3_column_type(mpVM, nCol);
}

bool CppSQLite3Query::eof() const {
    checkVM();
    return mbEof;
}

void CppSQLite3Query::nextRow() {
    checkVM();

    int nRet = sqlite3_step(mpVM);

    if (nRet == SQLITE_DONE) {
        // no rows
        mbEof = true;
    } else if (nRet == SQLITE_ROW) {
        // more rows, nothing to do
    } else {
        nRet = sqlite3_finalize(mpVM);
        mpVM = nullptr;
        const char *szError = sqlite3_errmsg(mpDB);
        throw CppSQLite3Exception(nRet, szError, DONT_DELETE_MSG);
    }
}

void CppSQLite3Query::finalize() {
    if (mpVM && mbOwnVM) {
        int nRet = sqlite3_finalize(mpVM);
        mpVM = nullptr;
        if (nRet != SQLITE_OK) {
            const char *szError = sqlite3_errmsg(mpDB);
            throw CppSQLite3Exception(nRet, szError, DONT_DELETE_MSG);
        }
    }
}

bool CppSQLite3Query::valid() const {
    return mpVM != nullptr;
}

void CppSQLite3Query::checkVM() const {
    if (mpVM == nullptr) {
        throw CppSQLite3Exception(
            CPPSQLITE_ERROR, "Null Virtual Machine pointer", DONT_DELETE_MSG);
    }
}

////////////////////////////////////////////////////////////////////////////////

CppSQLite3Table::CppSQLite3Table() {
    mpaszResults = nullptr;
    mnRows = 0;
    mnCols = 0;
    mnCurrentRow = 0;
}

CppSQLite3Table::CppSQLite3Table(const CppSQLite3Table &rTable) {
    mpaszResults = rTable.mpaszResults;
    // Only one object can own the results
    const_cast<CppSQLite3Table &>(rTable).mpaszResults = nullptr;
    mnRows = rTable.mnRows;
    mnCols = rTable.mnCols;
    mnCurrentRow = rTable.mnCurrentRow;
}

CppSQLite3Table::CppSQLite3Table(char **paszResults, int nRows, int nCols) {
    mpaszResults = paszResults;
    mnRows = nRows;
    mnCols = nCols;
    mnCurrentRow = 0;
}

CppSQLite3Table::~CppSQLite3Table() {
    try {
        finalize();
    } catch (...) {
    }
}

CppSQLite3Table &CppSQLite3Table::operator=(const CppSQLite3Table &rTable) {
    try {
        finalize();
    } catch (...) {
    }
    mpaszResults = rTable.mpaszResults;
    // Only one object can own the results
    const_cast<CppSQLite3Table &>(rTable).mpaszResults = nullptr;
    mnRows = rTable.mnRows;
    mnCols = rTable.mnCols;
    mnCurrentRow = rTable.mnCurrentRow;
    return *this;
}

void CppSQLite3Table::finalize() {
    if (mpaszResults) {
        sqlite3_free_table(mpaszResults);
        mpaszResults = nullptr;
    }
}

int CppSQLite3Table::numFields() const {
    checkResults();
    return mnCols;
}

int CppSQLite3Table::numRows() const {
    checkResults();
    return mnRows;
}

const char *CppSQLite3Table::fieldValue(int nField) const {
    checkResults();

    if (nField < 0 || nField >= mnCols) {
        throw CppSQLite3Exception(
            CPPSQLITE_ERROR, "Invalid field index requested", DONT_DELETE_MSG);
    }

    int nIndex = (mnCurrentRow * mnCols) + mnCols + nField;
    return mpaszResults[nIndex];
}

const char *CppSQLite3Table::fieldValue(const char *szField) const {
    checkResults();

    if (szField) {
        for (int nField = 0; nField < mnCols; nField++) {
            if (strcmp(szField, mpaszResults[nField]) == 0) {
                int nIndex = (mnCurrentRow * mnCols) + mnCols + nField;
                return mpaszResults[nIndex];
            }
        }
    }

    throw CppSQLite3Exception(CPPSQLITE_ERROR, "Invalid field name requested",
                              DONT_DELETE_MSG);
}

int CppSQLite3Table::getIntField(int nField, int nNullValue /*=0*/) const {
    if (fieldIsNull(nField)) {
        return nNullValue;
    } else {
        return atoi(fieldValue(nField));
    }
}

int CppSQLite3Table::getIntField(const char *szField,
                                 int nNullValue /*=0*/) const {
    if (fieldIsNull(szField)) {
        return nNullValue;
    } else {
        return atoi(fieldValue(szField));
    }
}

float CppSQLite3Table::getFloatField(int nField,
                                     float fNullValue /*=0.0f*/) const {
    if (fieldIsNull(nField)) {
        return fNullValue;
    } else {
        return static_cast<float>(atof(fieldValue(nField)));
    }
}

float CppSQLite3Table::getFloatField(const char *szField,
                                     float fNullValue /*=0.0f*/) const {
    if (fieldIsNull(szField)) {
        return fNullValue;
    } else {
        return static_cast<float>(atof(fieldValue(szField)));
    }
}

double CppSQLite3Table::getDoubleField(int nField,
                                       double dNullValue /*=0.0*/) const {
    if (fieldIsNull(nField)) {
        return dNullValue;
    } else {
        return atof(fieldValue(nField));
    }
}

double CppSQLite3Table::getDoubleField(const char *szField,
                                       double dNullValue /*=0.0*/) const {
    if (fieldIsNull(szField)) {
        return dNullValue;
    } else {
        return atof(fieldValue(szField));
    }
}

const char *
CppSQLite3Table::getStringField(int nField,
                                const char *szNullValue /*=""*/) const {
    if (fieldIsNull(nField)) {
        return szNullValue;
    } else {
        return fieldValue(nField);
    }
}

const char *
CppSQLite3Table::getStringField(const char *szField,
                                const char *szNullValue /*=""*/) const {
    if (fieldIsNull(szField)) {
        return szNullValue;
    } else {
        return fieldValue(szField);
    }
}

bool CppSQLite3Table::fieldIsNull(int nField) const {
    checkResults();
    return (fieldValue(nField) == nullptr);
}

bool CppSQLite3Table::fieldIsNull(const char *szField) const {
    checkResults();
    return (fieldValue(szField) == nullptr);
}

const char *CppSQLite3Table::fieldName(int nCol) const {
    checkResults();

    if (nCol < 0 || nCol >= mnCols) {
        throw CppSQLite3Exception(
            CPPSQLITE_ERROR, "Invalid field index requested", DONT_DELETE_MSG);
    }

    return mpaszResults[nCol];
}

void CppSQLite3Table::setRow(int nRow) {
    checkResults();

    if (nRow < 0 || nRow >= mnRows) {
        throw CppSQLite3Exception(
            CPPSQLITE_ERROR, "Invalid row index requested", DONT_DELETE_MSG);
    }

    mnCurrentRow = nRow;
}

void CppSQLite3Table::checkResults() const {
    if (mpaszResults == nullptr) {
        throw CppSQLite3Exception(CPPSQLITE_ERROR, "Null Results pointer",
                                  DONT_DELETE_MSG);
    }
}

////////////////////////////////////////////////////////////////////////////////

CppSQLite3Statement::CppSQLite3Statement() {
    mpDB = nullptr;
    mpVM = nullptr;
}

CppSQLite3Statement::CppSQLite3Statement(
    const CppSQLite3Statement &rStatement) {
    mpDB = rStatement.mpDB;
    mpVM = rStatement.mpVM;
    // Only one object can own VM
    const_cast<CppSQLite3Statement &>(rStatement).mpVM = nullptr;
}

CppSQLite3Statement::CppSQLite3Statement(sqlite3 *pDB, sqlite3_stmt *pVM) {
    mpDB = pDB;
    mpVM = pVM;
}

CppSQLite3Statement::~CppSQLite3Statement() {
    try {
        finalize();
    } catch (...) {
    }
}

CppSQLite3Statement &
CppSQLite3Statement::operator=(const CppSQLite3Statement &rStatement) {
    mpDB = rStatement.mpDB;
    mpVM = rStatement.mpVM;
    // Only one object can own VM
    const_cast<CppSQLite3Statement &>(rStatement).mpVM = nullptr;
    return *this;
}

int CppSQLite3Statement::execDML() {
    checkDB();
    checkVM();

    const char *szError = nullptr;

    int nRet = sqlite3_step(mpVM);

    if (nRet == SQLITE_DONE) {
        int nRowsChanged = sqlite3_changes(mpDB);

        nRet = sqlite3_reset(mpVM);

        if (nRet != SQLITE_OK) {
            szError = sqlite3_errmsg(mpDB);
            throw CppSQLite3Exception(nRet, szError, DONT_DELETE_MSG);
        }

        return nRowsChanged;
    } else {
        nRet = sqlite3_reset(mpVM);
        szError = sqlite3_errmsg(mpDB);
        throw CppSQLite3Exception(nRet, szError, DONT_DELETE_MSG);
    }
}

CppSQLite3Query CppSQLite3Statement::execQuery() {
    checkDB();
    checkVM();

    int nRet = sqlite3_step(mpVM);

    if (nRet == SQLITE_DONE) {
        // no rows
        return CppSQLite3Query(mpDB, mpVM, true /*eof*/, false);
    } else if (nRet == SQLITE_ROW) {
        // at least 1 row
        return CppSQLite3Query(mpDB, mpVM, false /*eof*/, false);
    } else {
        nRet = sqlite3_reset(mpVM);
        const char *szError = sqlite3_errmsg(mpDB);
        throw CppSQLite3Exception(nRet, szError, DONT_DELETE_MSG);
    }
}

void CppSQLite3Statement::bind(int nParam, const char *szValue) {
    checkVM();
    int nRes = sqlite3_bind_text(mpVM, nParam, szValue, -1, SQLITE_TRANSIENT);

    if (nRes != SQLITE_OK) {
        throw CppSQLite3Exception(nRes, "Error binding string param",
                                  DONT_DELETE_MSG);
    }
}

void CppSQLite3Statement::bind(int nParam, const int nValue) {
    checkVM();
    int nRes = sqlite3_bind_int(mpVM, nParam, nValue);

    if (nRes != SQLITE_OK) {
        throw CppSQLite3Exception(nRes, "Error binding int param",
                                  DONT_DELETE_MSG);
    }
}

void CppSQLite3Statement::bind(int nParam, const long long nValue) {
    checkVM();
    int nRes = sqlite3_bind_int64(mpVM, nParam, nValue);

    if (nRes != SQLITE_OK) {
        throw CppSQLite3Exception(nRes, "Error binding int64 param",
                                  DONT_DELETE_MSG);
    }
}

void CppSQLite3Statement::bind(int nParam, const double dValue) {
    checkVM();
    int nRes = sqlite3_bind_double(mpVM, nParam, dValue);

    if (nRes != SQLITE_OK) {
        throw CppSQLite3Exception(nRes, "Error binding double param",
                                  DONT_DELETE_MSG);
    }
}

void CppSQLite3Statement::bind(int nParam, const unsigned char *blobValue,
                               size_t nLen) {
    checkVM();
    int nRes =
        sqlite3_bind_blob(mpVM, nParam, static_cast<const void *>(blobValue),
                          static_cast<int>(nLen), SQLITE_TRANSIENT);

    if (nRes != SQLITE_OK) {
        throw CppSQLite3Exception(nRes, "Error binding blob param",
                                  DONT_DELETE_MSG);
    }
}

void CppSQLite3Statement::bindNull(int nParam) {
    checkVM();
    int nRes = sqlite3_bind_null(mpVM, nParam);

    if (nRes != SQLITE_OK) {
        throw CppSQLite3Exception(nRes, "Error binding NULL param",
                                  DONT_DELETE_MSG);
    }
}

void CppSQLite3Statement::reset() {
    if (mpVM) {
        int nRet = sqlite3_reset(mpVM);

        if (nRet != SQLITE_OK) {
            const char *szError = sqlite3_errmsg(mpDB);
            throw CppSQLite3Exception(nRet, szError, DONT_DELETE_MSG);
        }
    }
}

void CppSQLite3Statement::finalize() {
    if (mpVM) {
        int nRet = sqlite3_finalize(mpVM);
        mpVM = nullptr;

        if (nRet != SQLITE_OK) {
            const char *szError = sqlite3_errmsg(mpDB);
            throw CppSQLite3Exception(nRet, szError, DONT_DELETE_MSG);
        }
    }
}

void CppSQLite3Statement::checkDB() const {
    if (mpDB == nullptr) {
        throw CppSQLite3Exception(CPPSQLITE_ERROR, "Database not open",
                                  DONT_DELETE_MSG);
    }
}

void CppSQLite3Statement::checkVM() const {
    if (mpVM == nullptr) {
        throw CppSQLite3Exception(
            CPPSQLITE_ERROR, "Null Virtual Machine pointer", DONT_DELETE_MSG);
    }
}

////////////////////////////////////////////////////////////////////////////////

CppSQLite3DB::CppSQLite3DB() {
    mpDB = nullptr;
    mnBusyTimeoutMs = 60000; // 60 seconds
}

CppSQLite3DB::CppSQLite3DB(const CppSQLite3DB &db) {
    mpDB = db.mpDB;
    mnBusyTimeoutMs = 60000; // 60 seconds
}

CppSQLite3DB::~CppSQLite3DB() { close(); }

CppSQLite3DB &CppSQLite3DB::operator=(const CppSQLite3DB &db) {
    mpDB = db.mpDB;
    mnBusyTimeoutMs = 60000; // 60 seconds
    return *this;
}

void CppSQLite3DB::open(const char *szFile) {
    int nRet = sqlite3_open(szFile, &mpDB);

    if (nRet != SQLITE_OK) {
        const char *szError = sqlite3_errmsg(mpDB);
        throw CppSQLite3Exception(nRet, szError, DONT_DELETE_MSG);
    }

    setBusyTimeout(mnBusyTimeoutMs);
}


#if SQLITE_VERSION_NUMBER >= 3005000
void CppSQLite3DB::open(const char *szFile, int flags, const char *szVfsName) {
    int nRet = sqlite3_open_v2(szFile, &mpDB, flags, szVfsName);

    if (nRet != SQLITE_OK) {
        const char* szError = sqlite3_errmsg(mpDB);
        throw CppSQLite3Exception(nRet, szError, DONT_DELETE_MSG);
    }

    setBusyTimeout(mnBusyTimeoutMs);
}
#endif

void CppSQLite3DB::close() {
    if (mpDB) {
#if SQLITE_VERSION_NUMBER >= 3007014
        sqlite3_close_v2(mpDB);
#else
        sqlite3_close(mpDB);
#endif
        mpDB = nullptr;
    }
}

CppSQLite3Statement CppSQLite3DB::compileStatement(const char *szSQL) {
    checkDB();

    sqlite3_stmt *pVM = compile(szSQL);
    return CppSQLite3Statement(mpDB, pVM);
}

bool CppSQLite3DB::tableExists(const char *szTable) {
    CppSQLite3Buffer sql;
    sql.format(
        "select count(*) from sqlite_master where type='table' and name=%Q",
        szTable);
    int nRet = execScalar(sql);
    return (nRet > 0);
}


bool CppSQLite3DB::columnExists(const char *szTable, const char *szColumn) {
    CppSQLite3Buffer sql;
    sql.format("PRAGMA table_info(%Q)", szTable);

    CppSQLite3Query record = execQuery(sql);
    while (!record.eof()) {
        const char* colName = record.getStringField("name");
        if (stricmp(colName, szColumn) == 0) {
            return true;
        }
        record.nextRow();
    }

    return false;
}

int CppSQLite3DB::execDML(const char *szSQL) {
    checkDB();

    char *szError = nullptr;

    int nRet = sqlite3_exec(mpDB, szSQL, nullptr, nullptr, &szError);

    if (nRet == SQLITE_OK) {
        return sqlite3_changes(mpDB);
    } else {
        throw CppSQLite3Exception(nRet, szError);
    }
}

CppSQLite3Query CppSQLite3DB::execQuery(const char *szSQL) {
    checkDB();

    sqlite3_stmt *pVM = compile(szSQL);

    int nRet = sqlite3_step(pVM);

    if (nRet == SQLITE_DONE) {
        // no rows
        return CppSQLite3Query(mpDB, pVM, true /*eof*/);
    } else if (nRet == SQLITE_ROW) {
        // at least 1 row
        return CppSQLite3Query(mpDB, pVM, false /*eof*/);
    } else {
        nRet = sqlite3_finalize(pVM);
        const char *szError = sqlite3_errmsg(mpDB);
        throw CppSQLite3Exception(nRet, szError, DONT_DELETE_MSG);
    }
}

int CppSQLite3DB::execScalar(const char *szSQL, int nNullSentinel /*=0*/) {
    CppSQLite3Query q = execQuery(szSQL);

    if (q.eof() || q.numFields() < 1) {
        throw CppSQLite3Exception(CPPSQLITE_ERROR, "Invalid scalar query",
                                  DONT_DELETE_MSG);
    }

    return q.getIntField(0, nNullSentinel);
}

CppSQLite3Table CppSQLite3DB::getTable(const char *szSQL) {
    checkDB();

    char *szError = nullptr;
    char **paszResults = nullptr;
    int nRet;
    int nRows(0);
    int nCols(0);

    nRet =
        sqlite3_get_table(mpDB, szSQL, &paszResults, &nRows, &nCols, &szError);

    if (nRet == SQLITE_OK) {
        return CppSQLite3Table(paszResults, nRows, nCols);
    } else {
        throw CppSQLite3Exception(nRet, szError);
    }
}

sqlite_int64 CppSQLite3DB::lastRowId() const {
    return sqlite3_last_insert_rowid(mpDB);
}

void CppSQLite3DB::setBusyTimeout(int nMillisecs) {
    mnBusyTimeoutMs = nMillisecs;
    sqlite3_busy_timeout(mpDB, mnBusyTimeoutMs);
}

void CppSQLite3DB::checkDB() const {
    if (!mpDB) {
        throw CppSQLite3Exception(CPPSQLITE_ERROR, "Database not open",
                                  DONT_DELETE_MSG);
    }
}

sqlite3_stmt *CppSQLite3DB::compile(const char *szSQL) {
    checkDB();

    char *szError = nullptr;
    const char *szTail = nullptr;
    sqlite3_stmt *pVM;

    int nRet = sqlite3_prepare(mpDB, szSQL, -1, &pVM, &szTail);

    if (nRet != SQLITE_OK) {
        throw CppSQLite3Exception(nRet, szError);
    }

    return pVM;
}

////////////////////////////////////////////////////////////////////////////////
// SQLite encode.c reproduced here, containing implementation notes and source
// for sqlite3_encode_binary() and sqlite3_decode_binary()
////////////////////////////////////////////////////////////////////////////////

/*
** 2002 April 25
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains helper routines used to translate binary data into
** a null-terminated string (suitable for use in SQLite) and back again.
** These are convenience routines for use by people who want to store binary
** data in an SQLite database.  The code in this file is not used by any other
** part of the SQLite library.
**
** $Id: encode.c,v 1.10 2004/01/14 21:59:23 drh Exp $
*/

/*
** How This Encoder Works
**
** The output is allowed to contain any character except 0x27 (') and
** 0x00.  This is accomplished by using an escape character to encode
** 0x27 and 0x00 as a two-byte sequence.  The escape character is always
** 0x01.  An 0x00 is encoded as the two byte sequence 0x01 0x01.  The
** 0x27 character is encoded as the two byte sequence 0x01 0x03.  Finally,
** the escape character itself is encoded as the two-character sequence
** 0x01 0x02.
**
** To summarize, the encoder works by using an escape sequences as follows:
**
**       0x00  ->  0x01 0x01
**       0x01  ->  0x01 0x02
**       0x27  ->  0x01 0x03
**
** If that were all the encoder did, it would work, but in certain cases
** it could double the size of the encoded string.  For example, to
** encode a string of 100 0x27 characters would require 100 instances of
** the 0x01 0x03 escape sequence resulting in a 200-character output.
** We would prefer to keep the size of the encoded string smaller than
** this.
**
** To minimize the encoding size, we first add a fixed offset value to each
** byte in the sequence.  The addition is modulo 256.  (That is to say, if
** the sum of the original character value and the offset exceeds 256, then
** the higher order bits are truncated.)  The offset is chosen to minimize
** the number of characters in the string that need to be escaped.  For
** example, in the case above where the string was composed of 100 0x27
** characters, the offset might be 0x01.  Each of the 0x27 characters would
** then be converted into an 0x28 character which would not need to be
** escaped at all and so the 100 character input string would be converted
** into just 100 characters of output.  Actually 101 characters of output -
** we have to record the offset used as the first byte in the sequence so
** that the string can be decoded.  Since the offset value is stored as
** part of the output string and the output string is not allowed to contain
** characters 0x00 or 0x27, the offset cannot be 0x00 or 0x27.
**
** Here, then, are the encoding steps:
**
**     (1)   Choose an offset value and make it the first character of
**           output.
**
**     (2)   Copy each input character into the output buffer, one by
**           one, adding the offset value as you copy.
**
**     (3)   If the value of an input character plus offset is 0x00, replace
**           that one character by the two-character sequence 0x01 0x01.
**           If the sum is 0x01, replace it with 0x01 0x02.  If the sum
**           is 0x27, replace it with 0x01 0x03.
**
**     (4)   Put a 0x00 terminator at the end of the output.
**
** Decoding is obvious:
**
**     (5)   Copy encoded characters except the first into the decode
**           buffer.  Set the first encoded character aside for use as
**           the offset in step 7 below.
**
**     (6)   Convert each 0x01 0x01 sequence into a single character 0x00.
**           Convert 0x01 0x02 into 0x01.  Convert 0x01 0x03 into 0x27.
**
**     (7)   Subtract the offset value that was the first character of
**           the encoded buffer from all characters in the output buffer.
**
** The only tricky part is step (1) - how to compute an offset value to
** minimize the size of the output buffer.  This is accomplished by testing
** all offset values and picking the one that results in the fewest number
** of escapes.  To do that, we first scan the entire input and count the
** number of occurances of each character value in the input.  Suppose
** the number of 0x00 characters is N(0), the number of occurances of 0x01
** is N(1), and so forth up to the number of occurances of 0xff is N(255).
** An offset of 0 is not allowed so we don't have to test it.  The number
** of escapes required for an offset of 1 is N(1)+N(2)+N(40).  The number
** of escapes required for an offset of 2 is N(2)+N(3)+N(41).  And so forth.
** In this way we find the offset that gives the minimum number of escapes,
** and thus minimizes the length of the output string.
*/

/*
** Encode a binary buffer "in" of size n bytes so that it contains
** no instances of characters '\'' or '\000'.  The output is
** null-terminated and can be used as a string value in an INSERT
** or UPDATE statement.  Use sqlite3_decode_binary() to convert the
** string back into its original binary.
**
** The result is written into a preallocated output buffer "out".
** "out" must be able to hold at least 2 +(257*n)/254 bytes.
** In other words, the output will be expanded by as much as 3
** bytes for every 254 bytes of input plus 2 bytes of fixed overhead.
** (This is approximately 2 + 1.0118*n or about a 1.2% size increase.)
**
** The return value is the number of characters in the encoded
** string, excluding the "\000" terminator.
*/
int sqlite3_encode_binary(const unsigned char *in, int n, unsigned char *out) {
    int i, j, e = 0, m;
    int cnt[256];
    if (n <= 0) {
        out[0] = 'x';
        out[1] = 0;
        return 1;
    }
    memset(cnt, 0, sizeof(cnt));
    for (i = n - 1; i >= 0; i--) {
        cnt[in[i]]++;
    }
    m = n;
    for (i = 1; i < 256; i++) {
        int sum;
        if (i == '\'')
            continue;
        sum = cnt[i] + cnt[(i + 1) & 0xff] + cnt[(i + '\'') & 0xff];
        if (sum < m) {
            m = sum;
            e = i;
            if (m == 0)
                break;
        }
    }
    out[0] = static_cast<unsigned char>(e);
    j = 1;
    for (i = 0; i < n; i++) {
        int c = (in[i] - e) & 0xff;
        if (c == 0) {
            out[j++] = 1;
            out[j++] = 1;
        } else if (c == 1) {
            out[j++] = 1;
            out[j++] = 2;
        } else if (c == '\'') {
            out[j++] = 1;
            out[j++] = 3;
        } else {
            out[j++] = static_cast<unsigned char>(c);
        }
    }
    out[j] = 0;
    return j;
}

/*
** Decode the string "in" into binary data and write it into "out".
** This routine reverses the encoding created by sqlite3_encode_binary().
** The output will always be a few bytes less than the input.  The number
** of bytes of output is returned.  If the input is not a well-formed
** encoding, -1 is returned.
**
** The "in" and "out" parameters may point to the same buffer in order
** to decode a string in place.
*/
int sqlite3_decode_binary(const unsigned char *in, unsigned char *out) {
    int i, c, e;
    e = *(in++);
    i = 0;
    while ((c = *(in++)) != 0) {
        if (c == 1) {
            c = *(in++);
            if (c == 1) {
                c = 0;
            } else if (c == 2) {
                c = 1;
            } else if (c == 3) {
                c = '\'';
            } else {
                return -1;
            }
        }
        out[i++] = static_cast<unsigned char>((c + e) & 0xff);
    }
    return i;
}

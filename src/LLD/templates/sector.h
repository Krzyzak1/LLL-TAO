/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#pragma once
#ifndef NEXUS_LLD_TEMPLATES_SECTOR_H
#define NEXUS_LLD_TEMPLATES_SECTOR_H


#include <LLD/include/enum.h>
#include <LLD/include/version.h>
#include <LLD/templates/key.h>
#include <LLD/templates/transaction.h>

#include <LLD/cache/binary_lru.h>
#include <LLD/cache/template_lru.h>

#include <LLD/keychain/filemap.h>
#include <LLD/keychain/hashmap.h>
#include <LLD/keychain/hashtree.h>

#include <Util/templates/datastream.h>
#include <Util/include/runtime.h>
#include <Util/include/debug.h>

#include <string>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace LLD
{


    /* Maximum size a file can be in the keychain. */
    const uint32_t MAX_SECTOR_FILE_SIZE = 1024 * 1024 * 1024; //1 GB per File


    /* The maximum amount of bytes allowed in the memory buffer for disk flushes. **/
    const uint32_t MAX_SECTOR_BUFFER_SIZE = 1024 * 1024 * 4; //4 MB Max Disk Buffer


    /** SectorDatabase
     *
     *  Base Template Class for a Sector Database.
     *  Processes main Lower Level Disk Communications.
     *  A Sector Database Is a Fixed Width Data Storage Medium.
     *
     *  It is ideal for data structures to be stored that do not
     *  change in their size. This allows the greatest efficiency
     *  in fixed data storage (structs, class, etc.).
     *
     *  It is not ideal for data structures that may vary in size
     *  over their lifetimes. The Dynamic Database will allow that.
     *
     *  Key Type can be of any type. Data lengths are attributed to
     *  each key type. Keys are assigned sectors and stored in the
     *  key storage file. Sector files are broken into maximum of 1 GB
     *  for stability on all systems, key files are determined the same.
     *
     *  Multiple Keys can point back to the same sector to allow multiple
     *  access levels of the sector. This specific class handles the lower
     *  level disk communications for the sector database.
     *
     *  If each sector was allowed to be varying sizes it would remove the
     *  ability to use free space that becomes available upon an erase of a
     *  record. Use this Database purely for fixed size structures. Overflow
     *  attempts will trigger an error code.
     *
     **/
    template<class KeychainType, class CacheType>
    class SectorDatabase
    {
        /* The mutex for the condition. */
        std::mutex CONDITION_MUTEX;

        /* The condition for thread sleeping. */
        std::condition_variable CONDITION;

        /* Transaction file stream. */
        std::ofstream STREAM;

    protected:
        /* Mutex for Thread Synchronization.
            TODO: Lock Mutex based on Read / Writes on a per Sector Basis.
            Will allow higher efficiency for thread concurrency. */
        std::mutex SECTOR_MUTEX;
        std::mutex BUFFER_MUTEX;
        std::mutex TRANSACTION_MUTEX;


        /* The String to hold the Disk Location of Database File. */
        std::string strBaseLocation;
        std::string strName;


        /* timer for Runtime Calculations. */
        runtime::timer runtime;


        /* Class to handle Transaction Data. */
        SectorTransaction* pTransaction;


        /* Sector Keys Database. */
        KeychainType* pSectorKeys;


        /* Cache Pool */
        CacheType* cachePool;


        /* File stream object. */
        mutable TemplateLRU<uint32_t, std::fstream*>* fileCache;


        /* The current File Position. */
        mutable uint32_t nCurrentFile;
        mutable uint32_t nCurrentFileSize;


        /* Cache Writer Thread. */
        std::thread CacheWriterThread;


        /* The meter thread. */
        std::thread MeterThread;


        /* Disk Buffer Vector. */
        std::vector< std::pair< std::vector<uint8_t>, std::vector<uint8_t> > > vDiskBuffer;


        /* Disk Buffer Memory Size. */
        std::atomic<uint32_t> nBufferBytes;

        /* For the Meter. */
        std::atomic<uint32_t> nBytesRead;
        std::atomic<uint32_t> nBytesWrote;
        std::atomic<uint32_t> nRecordsFlushed;

        /* Destructor Flag. */
        std::atomic<bool> fDestruct;


        /* Initialize Flag. */
        std::atomic<bool> fInitialized;


        /** Database Flags. **/
        uint8_t nFlags;


    public:


        /** The Database Constructor. To determine file location and the Bytes per Record. **/
        SectorDatabase(const std::string& strNameIn, const uint8_t nFlagsIn,
                       const uint64_t nBucketsIn = 256 * 256 * 64, const uint32_t nCacheIn = 1024 * 1024);


        /** Default Destructor **/
        virtual ~SectorDatabase();


        /** Initialize
         *
         *  Initialize Sector Database.
         *
         **/
        void Initialize();


        /** Exists
         *
         *  Determine if the entry identified by the given key exists.
         *
         *  @param[in] key The key to the database entry.
         *
         *  @return True if the entry exists, false otherwise.
         *
         **/
        template<typename Key>
        bool Exists(const Key& key)
        {
            /* Serialize Key into Bytes. */
            DataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey << key;

            /* Check that the key is not pending in a transaction for Erase. */
            {
                LOCK(TRANSACTION_MUTEX);

                if(pTransaction)
                {
                    /* Check if in erase queue. */
                    if(pTransaction->mapEraseData.count(ssKey.Bytes()))
                        return false;

                    /* Check if the new data is set in a transaction to ensure that the database knows what is in volatile memory. */
                    if(pTransaction->mapTransactions.count(ssKey.Bytes()))
                        return true;

                    /* Check for keychain commits. */
                    if(pTransaction->mapKeychain.count(ssKey.Bytes()))
                        return true;

                    /* Check for the indexes. */
                    if(pTransaction->mapIndex.count(ssKey.Bytes()))
                        return true;
                }
            }

            /* Check the cache pool. */
            if(cachePool->Has(ssKey.Bytes()))
                return true;

            /* Return the Key existance in the Keychain Database. */
            SectorKey cKey;
            return pSectorKeys->Get(ssKey.Bytes(), cKey);
        }


        /** Erase
         *
         *  Erase a database entry identified by the given key.
         *
         *  @param[in] key The key to the database entry to erase.
         *
         *  @return True if the entry erased, false otherwise.
         *
         **/
        template<typename Key>
        bool Erase(const Key& key)
        {
            if(nFlags & FLAGS::READONLY)
                return debug::error("Erase called on database in read-only mode");

            /* Serialize Key into Bytes. */
            DataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey << key;

            /* Remove the item from the cache pool. */
            cachePool->Remove(ssKey.Bytes());

            /* Add transaction to erase queue. */
            {
                LOCK(TRANSACTION_MUTEX);

                if(pTransaction)
                {
                    /* Serialize the key. */
                    DataStream ssJournal(SER_LLD, DATABASE_VERSION);
                    ssJournal << std::string("erase") << ssKey.Bytes();

                    /* Write to the file.  */
                    const std::vector<uint8_t>& vBytes = ssJournal.Bytes();
                    STREAM.write((char*)&vBytes[0], vBytes.size());

                    /* Erase the transaction data. */
                    pTransaction->EraseTransaction(ssKey.Bytes());

                    return true;
                }
            }

            /* Return the Key existance in the Keychain Database. */
            return pSectorKeys->Erase(ssKey.Bytes());
        }


        /** Read
         *
         *  Read a database entry identified by the given key.
         *
         *  @param[in] key The key to the database entry to read.
         *  @param[out] value The database entry value to read out.
         *
         *  @return True if the entry read, false otherwise.
         *
         **/
        template<typename Type>
        bool BatchRead(const std::string& strType, std::vector<Type>& vValues, int32_t nLimit = 1000)
        {
            /* The current file being read. */
            uint32_t nFile = 0;

            /* Scan until limit is reached. */
            while(nLimit == -1 || nLimit > 0)
            {
                /* Get filestream object. */
                std::fstream stream = std::fstream(debug::strprintf("%s_block.%05u", strBaseLocation.c_str(), nFile), std::ios::in | std::ios::binary);
                if(!stream.is_open())
                    return (vValues.size() > 0);

                /* Read into serialize stream. */
                DataStream ssData(SER_LLD, DATABASE_VERSION);

                /* Get the current file position. */
                uint64_t nStart = 0;

                /* Get the Binary Size. */
                uint64_t nFileSize = 0;

                /* Read file data. */
                {
                    LOCK(SECTOR_MUTEX);

                    /* Get the Binary Size. */
                    stream.seekg(0, std::ios::end);
                    nFileSize = stream.tellg();

                    /* Get the Binary Size. */
                    uint64_t nBufferSize = ((nLimit == -1) ? nFileSize : (1024 * nLimit));

                    /* Check for exceeding actual size. */
                    if(nBufferSize > nFileSize)
                        nBufferSize = nFileSize;

                    /* Seek to beginning. */
                    stream.seekg(0, std::ios::beg);

                    /* Resize the buffer. */
                    ssData.resize(nBufferSize);

                    /* Read the data into the buffer. */
                    stream.read((char*)ssData.data(), ssData.size());

                    /* Set the start to read size. */
                    nStart += ssData.size();
                }

                /* Get the current position. */
                uint64_t nPos = 0;

                /* Read records. */
                while(!ssData.End())
                {
                    try
                    {
                        /* Read compact size. */
                        uint64_t nSize = ReadCompactSize(ssData);
                        if(nSize == 0)
                            continue;

                        /* Check for failures or serialization issues. */
                        if(nPos + nSize + GetSizeOfCompactSize(nSize) > nFileSize)
                            break;

                        /* Deserialize the String. */
                        std::string strThis;
                        ssData >> strThis;

                        /* Check the type. */
                        if(strType == strThis && strThis != "NONE")
                        {
                            /* Get the value. */
                            Type value;
                            ssData >> value;

                            /* Push next value. */
                            vValues.push_back(value);

                            /* Check limits. */
                            if(nLimit != -1 && --nLimit == 0)
                                return (vValues.size() > 0);
                        }

                        /* Iterate to next position. */
                        nPos += nSize + GetSizeOfCompactSize(nSize);
                        ssData.SetPos(nPos);

                    }
                    catch(const std::exception& e)
                    {
                        /* Read file data. */
                        {
                            LOCK(SECTOR_MUTEX);

                            /* Get the Binary Size. */
                            uint64_t nBufferSize = (1024 * nLimit);

                            /* Check for exceeding of buffer size. */
                            if(nStart + nBufferSize > nFileSize)
                                nBufferSize = (nFileSize - nStart);

                            /* Check for end. */
                            if(nBufferSize == 0)
                                break;

                            /* Seek stream to beginning. */
                            stream.seekg(nStart, std::ios::beg);
                            ssData.resize(ssData.size() + nBufferSize);

                            /* Read the data into the buffer. */
                            stream.read((char*)ssData.data(ssData.size() - nBufferSize), nBufferSize);

                            /* Set the start to read size. */
                            nStart += nBufferSize;

                            /* Reset the position. */
                            ssData.SetPos(nPos);
                        }
                    }
                }

                /* Close the stream. */
                stream.close();

                /* Iterate to the next file. */
                ++nFile;
            }

            return (vValues.size() > 0);
        }


        /** Read
         *
         *  Read a database entry identified by the given key.
         *Processed
         *  @param[in] key The key to the database entry to read.
         *  @param[out] value The database entry value to read out.
         *
         *  @return True if the entry read, false otherwise.
         *
         **/
        template<typename Key, typename Type>
        bool BatchRead(const Key& key, const std::string& strType, std::vector<Type>& vValues, int32_t nLimit = 1000)
        {
            /* Serialize Key into Bytes. */
            DataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey << key;

            /* Get the key. */
            SectorKey cKey;
            if(!pSectorKeys->Get(ssKey.Bytes(), cKey))
                return false;

            /* The current file being read. */
            uint32_t nFile = cKey.nSectorFile;

            /* Scan until limit is reached. */
            while(nLimit == -1 || nLimit > 0)
            {
                /* Get filestream object. */
                std::fstream stream = std::fstream(debug::strprintf("%s_block.%05u",
                                        strBaseLocation.c_str(), nFile), std::ios::in | std::ios::binary);
                if(!stream.is_open())
                    return (vValues.size() > 0);

                /* Read into serialize stream. */
                DataStream ssData(SER_LLD, DATABASE_VERSION);

                /* Get the current position. */
                uint64_t nPos = 0;

                /* Get the current file position. */
                uint64_t nStart = 0;

                /* Get the Binary Size. */
                uint64_t nFileSize = 0;

                /* Read file data. */
                {
                    LOCK(SECTOR_MUTEX);

                    /* Get the Binary Size. */
                    stream.seekg(0, std::ios::end);
                    nFileSize = stream.tellg();

                    /* Get the Binary Size. */
                    uint64_t nBufferSize = ((nLimit == -1) ? nFileSize : (1024 * nLimit));

                    /* Check for exceeding actual size. */
                    if(nBufferSize > nFileSize)
                        nBufferSize = nFileSize;

                    /* Seek to the key's binary location. */
                    if(nFile == cKey.nSectorFile)
                    {
                        /* Set the position. */
                        nStart = cKey.nSectorStart + cKey.nSectorSize;

                        /* Seek stream to sector position. */
                        stream.seekg(nStart, std::ios::beg);
                        ssData.resize(nBufferSize);
                    }

                    /* Otherwise seek to beginning if next file. */
                    else
                    {
                        /* Seek stream to beginning. */
                        stream.seekg(0, std::ios::beg);
                        ssData.resize(nBufferSize);
                    }

                    /* Read the data into the buffer. */
                    stream.read((char*)ssData.data(), nBufferSize);

                    /* Set the start to read size. */
                    nStart += ssData.size();
                }

                /* Read records. */
                while(!ssData.End())
                {
                    try
                    {
                        /* Read compact size. */
                        uint64_t nSize = ReadCompactSize(ssData);
                        if(nSize == 0)
                            continue;

                        /* Check for failures or serialization issues. */
                        if((nPos + nSize + GetSizeOfCompactSize(nSize)) > nFileSize)
                            break;

                        /* Deserialize the String. */
                        std::string strThis;
                        ssData >> strThis;

                        /* Check the type. */
                        if(strType == strThis && strThis != "NONE")
                        {
                            /* Get the value. */
                            Type value;
                            ssData >> value;

                            /* Push next value. */
                            vValues.push_back(value);

                            /* Check limits. */
                            if(nLimit != -1 && --nLimit == 0)
                                return (vValues.size() > 0);
                        }

                        /* Iterate to next position. */
                        nPos += nSize + GetSizeOfCompactSize(nSize);
                        ssData.SetPos(nPos);

                    }
                    catch(const std::exception& e)
                    {
                        /* Read file data. */
                        {
                            LOCK(SECTOR_MUTEX);

                            /* Get the Binary Size. */
                            uint64_t nBufferSize = (1024 * nLimit);

                            /* Check for exceeding of buffer size. */
                            if(nStart + nBufferSize > nFileSize)
                                nBufferSize = (nFileSize - nStart);

                            /* Check for end. */
                            if(nBufferSize == 0)
                                break;

                            /* Seek stream to beginning. */
                            stream.seekg(nStart, std::ios::beg);
                            ssData.resize(ssData.size() + nBufferSize);

                            /* Read the data into the buffer. */
                            stream.read((char*)ssData.data(ssData.size() - nBufferSize), nBufferSize);

                            /* Set the start to read size. */
                            nStart += nBufferSize;

                            /* Reset the position. */
                            ssData.SetPos(nPos);
                        }
                    }
                }

                /* Close the stream. */
                stream.close();

                /* Iterate to the next file. */
                ++nFile;
            }

            return (vValues.size() > 0);
        }


        /** Read
         *
         *  Read a database entry identified by the given key.
         *
         *  @param[in] key The key to the database entry to read.
         *  @param[out] value The database entry value to read out.
         *
         *  @return True if the entry read, false otherwise.
         *
         **/
        template<typename Key, typename Type>
        bool Read(const Key& key, Type& value)
        {
            /* Serialize Key into Bytes. */
            DataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey << key;

            /* Get the Data from Sector Database. */
            std::vector<uint8_t> vData;

            /* Check that the key is not pending in a transaction for Erase. */
            {
                LOCK(TRANSACTION_MUTEX);

                if(pTransaction)
                {
                    /* Check if in erase queue. */
                    if(pTransaction->mapEraseData.count(ssKey.Bytes()))
                        return false;

                    /* Check if the new data is set in a transaction to ensure that the database knows what is in volatile memory. */
                    if(pTransaction->mapTransactions.count(ssKey.Bytes()))
                    {
                        /* Get the data from the transction object. */
                        vData = pTransaction->mapTransactions[ssKey.Bytes()];

                        /* Deserialize Value. */
                        DataStream ssValue(vData, SER_LLD, DATABASE_VERSION);

                        /* Deserialize the String. */
                        std::string strType;
                        ssValue >> strType;

                        /* Deseriazlie the Value. */
                        ssValue >> value;

                        return true;
                    }
                }
            }

            /* Get the data from the database. */
            if(!Get(ssKey.Bytes(), vData))
                return false;

            /* Deserialize Value. */
            DataStream ssValue(vData, SER_LLD, DATABASE_VERSION);

            /* Deserialize the String. */
            std::string strType;
            ssValue >> strType;

            /* Deseriazlie the Value. */
            ssValue >> value;

            return true;
        }


        /** Index
         *
         *  Indexes a key into memory.
         *
         *  @param[in] key The key to index.
         *  @param[in] index The index of the key to write to.
         *
         *  @return True if successful indexing, false otherwise.
         *
         **/
        template<typename Key, typename Type>
        bool Index(const Key& key, const Type& index)
        {
            /* Serialize Key into Bytes. */
            DataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey << key;

            /* Serialize the index into bytes. */
            DataStream ssIndex(SER_LLD, DATABASE_VERSION);
            ssIndex << index;

            /* Check that the key is not pending in a transaction for Erase. */
            {
                LOCK(TRANSACTION_MUTEX);

                if(pTransaction)
                {
                    /* Serialize the key. */
                    DataStream ssJournal(SER_LLD, DATABASE_VERSION);
                    ssJournal << std::string("index") << ssKey.Bytes() << ssIndex.Bytes();

                    /* Write to the file.  */
                    const std::vector<uint8_t>& vBytes = ssJournal.Bytes();
                    STREAM.write((char*)&vBytes[0], vBytes.size());

                    /* Check if the new data is set in a transaction to ensure that the database knows what is in volatile memory. */
                    pTransaction->mapIndex[ssKey.Bytes()] = ssIndex.Bytes();

                    return true;
                }
            }

            /* Get the key. */
            SectorKey cKey;
            if(!pSectorKeys->Get(ssIndex.Bytes(), cKey))
                return false;

            /* Write the new sector key. */
            cKey.SetKey(ssKey.Bytes());
            return pSectorKeys->Put(cKey);
        }


        /** Write
         *
         *  Writes a key to the sector database
         *
         *  @param[in] key The key to write.
         *
         *  @return True if successful write, false otherwise.
         *
         **/
        template<typename Key>
        bool Write(const Key& key)
        {
            if(nFlags & FLAGS::READONLY)
                return debug::error(FUNCTION, "Write called on database in read-only mode");

            /* Serialize Key into Bytes. */
            DataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey << key;

            /* Check for transaction. */
            {
                LOCK(TRANSACTION_MUTEX);

                if(pTransaction)
                {
                    /* Serialize the key. */
                    DataStream ssJournal(SER_LLD, DATABASE_VERSION);
                    ssJournal << std::string("key") << ssKey.Bytes();

                    /* Write to the file.  */
                    const std::vector<uint8_t>& vBytes = ssJournal.Bytes();
                    STREAM.write((char*)&vBytes[0], vBytes.size());

                    /* Check if data is in erase queue, if so remove it. */
                    if(pTransaction->mapEraseData.count(ssKey.Bytes()))
                        pTransaction->mapEraseData.erase(ssKey.Bytes());

                    /* Set the transaction data. */
                    pTransaction->mapKeychain[ssKey.Bytes()] = 0;

                    return true;
                }
            }

            /* Return the Key existance in the Keychain Database. */
            SectorKey cKey(STATE::READY, ssKey.Bytes(), 0, 0, 0);
            return pSectorKeys->Put(cKey);
        }


        /** Write
         *
         *  Writes a key/value pair to the sector database
         *
         *  @param[in] key The key to write.
         *  @param[in] value The value to write.
         *
         *  @return True if successful write, false otherwise.
         *
         **/
        template<typename Key, typename Type>
        bool Write(const Key& key, const Type& value, const std::string& strType = "NONE")
        {
            if(nFlags & FLAGS::READONLY)
                return debug::error(FUNCTION, "Write called on database in read-only mode");

            /* Serialize the Key. */
            DataStream ssKey(SER_LLD, DATABASE_VERSION);
            ssKey << key;

            /* Serialize the Value */
            DataStream ssData(SER_LLD, DATABASE_VERSION);
            ssData << strType;
            ssData << value;

            /* Check for transaction. */
            {
                LOCK(TRANSACTION_MUTEX);

                if(pTransaction)
                {
                    /* Serialize the key. */
                    DataStream ssJournal(SER_LLD, DATABASE_VERSION);
                    ssJournal << std::string("write") << ssKey.Bytes() << ssData.Bytes();

                    /* Write to the file.  */
                    const std::vector<uint8_t>& vBytes = ssJournal.Bytes();
                    STREAM.write((char*)&vBytes[0], vBytes.size());

                    /* Check if data is in erase queue, if so remove it. */
                    if(pTransaction->mapEraseData.count(ssKey.Bytes()))
                        pTransaction->mapEraseData.erase(ssKey.Bytes());

                    /* Set the transaction data. */
                    pTransaction->mapTransactions[ssKey.Bytes()] = ssData.Bytes();

                    /* Handle for a record update that needs to be reverted in case of errors. */
                    //if(pSectorKeys->Get(ssKey.Bytes()))
                    //{
                        /* Read the data if it exists. */
                    //    std::vector<uint8_t> vData;
                    //    if(!Get(ssKey.Bytes(), vData))
                    //        return false;

                        /* Add the original data to the transaction. */
                    //    pTransaction->mapOriginalData[ssKey.Bytes()] = vData;
                    //}

                    return true;
                }
            }

            return Put(ssKey.Bytes(), ssData.Bytes());
        }


        /** Get
         *
         *  Get a record from cache or from disk
         *
         *  @param[in] vKey The binary data of the key to get.
         *  @param[out] vData The binary data of the record to get.
         *
         *  @return True if the record was read successfully.
         *
         **/
        bool Get(const std::vector<uint8_t>& vKey, std::vector<uint8_t>& vData);


        /** Get
         *
         *  Get a record from from disk if the sector key
         *  is already read from the keychain.
         *
         *  @param[in] cKey The sector key from keychain.
         *  @param[in] vData The binary data of the record to get.WRITE
         *
         *  @return True if the record was read successfully.
         *
         **/
        bool Get(const SectorKey& cKey, std::vector<uint8_t>& vData);


        /** Update
         *
         *  Update a record on disk.
         *
         *  @param[in] vKey The binary data of the key to flush
         *  @param[in] vData The binary data of the record to flush
         *
         *  @return True if the flush was successful.
         *
         **/
        bool Update(const std::vector<uint8_t>& vKey, const std::vector<uint8_t>& vData);


        /** Force
         *
         *  Force a write to disk immediately bypassing write buffers.
         *
         *  @param[in] vKey The binary data of the key to flush
         *  @param[in] vData The binary data of the record to flush
         *
         *  @return True if the flush was successful.
         *
         **/
        bool Force(const std::vector<uint8_t>& vKey, const std::vector<uint8_t>& vData);

        /** Put
         *
         *  Write a record into the cache and disk buffer for flushing to disk.
         *
         *  @param[in] vKey The binary data of the key to put.
         *  @param[in] vData The binary data of the record to put.
         *
         *  @return True if the record was written successfully.
         *
         **/
        bool Put(const std::vector<uint8_t>& vKey, const std::vector<uint8_t>& vData);


        /** CacheWriter
         *
         *  Flushes periodically data from the cache buffer to disk.
         *
         **/
        void CacheWriter();


        /** Meter
         *
         *  LLD Meter Thread. Tracks the Reads/Writes per second.
         *
         **/
        void Meter();


        /** TxnBegin
         *
         *  Start a database transaction.
         *
         **/
        void TxnBegin();


        /** TxnRollback
         *
         *  Rollback the transaction to previous state.
         *
         **/
        void TxnRollback();


        /** TxnCheckpoint
         *
         *  Write the transaction commitment message.
         *
         **/
        bool TxnCheckpoint();


        /** TxnRelease
         *
         *  Release the transaction checkpoint.
         *
         **/
        void TxnRelease();


        /** TxnCommit
         *
         *  Commit data from transaction object.
         *
         *  @return True, if commit is successful, false otherwise.
         *
         **/
        bool TxnCommit();


        /** TxnRecovery
         *
         *  Recover a transaction from the journal.
         *
         **/
        bool TxnRecovery();

    };
}

#endif

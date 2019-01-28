/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2018

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#ifndef NEXUS_LLD_INCLUDE_TRUST_H
#define NEXUS_LLD_INCLUDE_TRUST_H

#include <LLC/types/uint1024.h>

#include <LLD/include/version.h>
#include <LLD/templates/sector.h>

#include <LLD/cache/binary_lru.h>
#include <LLD/keychain/filemap.h>

#include <TAO/Ledger/types/trustkey.h>


namespace LLD
{


    class TrustDB : public SectorDatabase<BinaryFileMap, BinaryLRU>
    {

    public:
        /** The Database Constructor. To determine file location and the Bytes per Record. **/
        TrustDB(uint8_t nFlagsIn = FLAGS::CREATE | FLAGS::WRITE)
        : SectorDatabase("trust", nFlagsIn) { }


        /** Write Trust Key
         *
         *  Writes a trust key to the ledger DB.
         *
         *  @param[in] hashKey The key of trust key to write.
         *  @param[in] key The trust key object
         *
         *  @return True if the trust key was successfully written.
         *
         **/
        bool WriteTrustKey(const uint576_t& hashKey, const TAO::Ledger::TrustKey& key)
        {
            return Write(hashKey, key);
        }


        /** Read Trust Key
         *
         *  Reads a trust key from the ledger DB.
         *
         *  @param[in] hashKey The key of trust key to write.
         *  @param[in] key The trust key object
         *
         *  @return True if the trust key was successfully written.
         *
         **/
        bool ReadTrustKey(const uint576_t& hashKey, TAO::Ledger::TrustKey& key)
        {
            return Read(hashKey, key);
        }
    };
}

#endif

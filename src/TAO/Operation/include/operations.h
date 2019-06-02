/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#pragma once
#ifndef NEXUS_TAO_OPERATION_INCLUDE_OPERATIONS_H
#define NEXUS_TAO_OPERATION_INCLUDE_OPERATIONS_H

#include <TAO/Register/types/stream.h>
#include <TAO/Ledger/types/transaction.h>

/* Global TAO namespace. */
namespace TAO
{

    /* Operation Layer namespace. */
    namespace Operation
    {

        /** Write
         *
         *  Writes data to a register.
         *
         *  @param[in] hashAddress The register address to write to.
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Write(const uint256_t& hashAddress, const std::vector<uint8_t>& vchData,
            const uint8_t nFlags, TAO::Ledger::Transaction &tx);


        /** Append
         *
         *  Appends data to a register.
         *
         *  @param[in] hashAddress The register address to write to.
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Append(const uint256_t& hashAddress, const std::vector<uint8_t>& vchData,
            const uint8_t nFlags, TAO::Ledger::Transaction &tx);


        /** Register
         *
         *  Creates a new register if it doesn't exist.
         *
         *  @param[in] hashAddress The register address to create.
         *  @param[in] nType The type of register being written.
         *  @param[in] vchData The binary data to record in register.
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Register(const uint256_t& hashAddress, const uint8_t nType,
            const std::vector<uint8_t>& vchData, const uint8_t nFlags,
            TAO::Ledger::Transaction &tx);


        /** Transfer
         *
         *  Transfers a register between sigchains.
         *
         *  @param[in] hashAddress The register address to transfer.
         *  @param[in] hashTransfer The register to transfer to.
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Transfer(const uint256_t& hashAddress, const uint256_t& hashTransfer,
            const uint8_t nFlags, TAO::Ledger::Transaction &tx);

        /** Transfer
         *
         *  Transfers a register between sigchains.
         *
         *  @param[in] hashTx The tx that is being claimed.
         *  @param[in] hashAddress The register address to claim.
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Claim(const uint512_t& hashTx, const uint8_t nFlags, TAO::Ledger::Transaction &tx);


        /** Debit
         *
         *  Authorizes funds from an account to an account
         *
         *  @param[in] hashFrom The account being transferred from.
         *  @param[in] hashTo The account being transferred to.
         *  @param[in] nAmount The amount being transferred
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Debit(const uint256_t& hashFrom, const uint256_t& hashTo, const uint64_t nAmount,
            const uint8_t nFlags, TAO::Ledger::Transaction &tx);


        /** Credit
         *
         *  Commits funds from an account to an account
         *
         *  @param[in] hashTx The account being transferred from.
         *  @param[in] hashProof The proof address used in this credit.
         *  @param[in] hashTo The account being transferred to.
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Credit(const uint512_t& hashTx, const uint256_t& hashProof,
            const uint256_t& hashTo, const uint8_t nFlags,
            TAO::Ledger::Transaction &tx);


        /** Coinbase
         *
         *  Commits funds from a coinbase transaction
         *
         *  @param[in] hashAccount The account being transferred to.
         *  @param[in] nAmount The amount being transferred
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Coinbase(const uint256_t& hashAccount, const uint64_t nAmount,
            const uint8_t nFlags, TAO::Ledger::Transaction &tx);


        /** Trust
         *
         *  Coinstake operation for a trust transaction.
         *
         *  @param[in] hashLastTrust The last stake transaction for the trust account register.
         *  @param[in] nTrustScore The new account trust score after the operation.
         *  @param[in] nCoinstakeReward Coinstake reward paid to register by this operation
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Trust(const uint512_t& hashLastTrust, const uint64_t nTrustScore, const uint64_t nCoinstakeReward,
            const uint8_t nFlags, TAO::Ledger::Transaction &tx);


        /** Genesis
         *
         *  Coinstake operation for a genesis transaction.
         *
         *  @param[in] hashAddress The address of the trust account register
         *  @param[in] nCoinstakeReward Coinstake reward paid to register by this operation
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Genesis(const uint256_t& hashAddress, const uint64_t nCoinstakeReward,
            const uint8_t nFlags, TAO::Ledger::Transaction &tx);


        /** Stake
         *
         *  Move balance to pending stake for trust account.
         *
         *  @param[in] hashAddress The address of the trust account register
         *  @param[in] nAmount The amount of balance to stake
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Stake(const uint256_t& hashAddress, const uint64_t nAmount,
            const uint8_t nFlags, TAO::Ledger::Transaction &tx);


        /** Unstake
         *
         *  Move from stake to balance for trust account.
         *
         *  This operation moves from pending stake first. If nAmount exceeds the
         *  current pending stake, it moves the remainder from locked stake and applies the trust penalty.
         *
         *  @param[in] hashAddress The address of the trust account register
         *  @param[in] nAmount The amount of balance to unstake
         *  @param[in] nTrustPenalty The amount of trust reduction from removing stake balance.
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Unstake(const uint256_t& hashAddress, const uint64_t nAmount, const uint64_t nTrustPenalty,
            const uint8_t nFlags, TAO::Ledger::Transaction &tx);


        /** Authorize
         *
         *  Authorizes an action if holder of a token.
         *
         *  @param[in] hashTx The transaction being authorized for.
         *  @param[in] hashProof The register temporal proof to use.
         *  @param[in] nFlags The flag to determine if database state should be written.
         *  @param[out] tx The transaction calling operations
         *
         *  @return true if successful.
         *
         **/
        bool Authorize(const uint512_t& hashTx, const uint256_t& hashProof,
            const uint8_t nFlags, TAO::Ledger::Transaction &tx);
    }
}

#endif

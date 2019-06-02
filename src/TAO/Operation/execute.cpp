/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "Man often becomes what he believes himself to be." - Mahatma Gandhi

____________________________________________________________________________________________*/


#include <LLC/types/uint1024.h>

#include <TAO/Operation/include/execute.h>
#include <TAO/Operation/include/stream.h>
#include <TAO/Operation/include/enum.h>
#include <TAO/Operation/include/operations.h>

#include <TAO/Register/include/enum.h>

#include <TAO/Ledger/types/transaction.h>

#include <Util/include/hex.h>



namespace TAO
{
    namespace Operation
    {

        /* Executes a given operation byte sequence. */
        bool Execute(TAO::Ledger::Transaction& tx, uint8_t nFlags)
        {

            /* Start the stream at the beginning. */
            tx.ssOperation.seek(0, STREAM::BEGIN);

            /* Start the register stream at the beginning. */
            tx.ssRegister.seek(0, STREAM::BEGIN);

            /* Make sure no exceptions are thrown. */
            try
            {

                /* Loop through the operations tx.ssOperation. */
                while(!tx.ssOperation.end())
                {
                    uint8_t OPERATION;
                    tx.ssOperation >> OPERATION;

                    /* Check the current opcode. */
                    switch(OPERATION)
                    {

                        /* Record a new state to the register. */
                        case OP::WRITE:
                        {
                            /* Get the Address of the Register. */
                            uint256_t hashAddress;
                            tx.ssOperation >> hashAddress;

                            /* Deserialize the register from tx.ssOperation. */
                            std::vector<uint8_t> vchData;
                            tx.ssOperation >> vchData;

                            /* Execute the operation method. */
                            if(!Write(hashAddress, vchData, nFlags, tx))
                                return false;

                            break;
                        }


                        /* Append a new state to the register. */
                        case OP::APPEND:
                        {
                            /* Get the Address of the Register. */
                            uint256_t hashAddress;
                            tx.ssOperation >> hashAddress;

                            /* Deserialize the register from tx.ssOperation. */
                            std::vector<uint8_t> vchData;
                            tx.ssOperation >> vchData;

                            /* Execute the operation method. */
                            if(!Append(hashAddress, vchData, nFlags, tx))
                                return false;

                            break;
                        }


                        /* Create a new register. */
                        case OP::REGISTER:
                        {
                            /* Extract the address from the tx.ssOperation. */
                            uint256_t hashAddress;
                            tx.ssOperation >> hashAddress;

                            /* Extract the register type from tx.ssOperation. */
                            uint8_t nType;
                            tx.ssOperation >> nType;

                            /* Extract the register data from the tx.ssOperation. */
                            std::vector<uint8_t> vchData;
                            tx.ssOperation >> vchData;

                            /* Execute the operation method. */
                            if(!Register(hashAddress, nType, vchData, nFlags, tx))
                                return false;

                            break;
                        }


                        /* Transfer ownership of a register to another signature chain. */
                        case OP::TRANSFER:
                        {
                            /* Extract the address from the tx.ssOperation. */
                            uint256_t hashAddress;
                            tx.ssOperation >> hashAddress;

                            /* Read the register transfer recipient. */
                            uint256_t hashTransfer;
                            tx.ssOperation >> hashTransfer;

                            /* Execute the operation method. */
                            if(!Transfer(hashAddress, hashTransfer, nFlags, tx))
                                return false;

                            break;
                        }


                        /* Transfer ownership of a register to another signature chain. */
                        case OP::CLAIM:
                        {
                            /* The transaction that this transfer is claiming. */
                            uint512_t hashTx;
                            tx.ssOperation >> hashTx;

                            /* Execute the operation method. */
                            if(!Claim(hashTx, nFlags, tx))
                                return false;

                            break;
                        }


                        /* Debit tokens from an account you own. */
                        case OP::DEBIT:
                        {
                            uint256_t hashAddress; //the register address debit is being sent from. Hard reject if this register isn't account id
                            tx.ssOperation >> hashAddress;

                            uint256_t hashTransfer;   //the register address debit is being sent to. Hard reject if this register isn't an account id
                            tx.ssOperation >> hashTransfer;

                            uint64_t  nAmount;  //the amount to be transfered
                            tx.ssOperation >> nAmount;

                            /* Execute the operation method. */
                            if(!Debit(hashAddress, hashTransfer, nAmount, nFlags, tx))
                                return false;

                            break;
                        }


                        /* Credit tokens to an account you own. */
                        case OP::CREDIT:
                        {
                            /* The transaction that this credit is claiming. */
                            uint512_t hashTx;
                            tx.ssOperation >> hashTx;

                            /* The proof this credit is using to make claims. */
                            uint256_t hashProof;
                            tx.ssOperation >> hashProof;

                            /* The account that is being credited. */
                            uint256_t hashAccount;
                            tx.ssOperation >> hashAccount;

                            /* Execute the operation method. */
                            if(!Credit(hashTx, hashProof, hashAccount, nFlags, tx))
                                return false;

                            break;
                        }


                        /* Coinbase operation. Creates an account if none exists. */
                        case OP::COINBASE:
                        {
                            /* Ensure that it as beginning of the tx.ssOperation. */
                            if(!tx.ssOperation.begin())
                                return debug::error(FUNCTION, "coinbase operation has to be first");

                            /* The total to be credited. */
                            uint64_t  nCredit;
                            tx.ssOperation >> nCredit;

                            /* The extra nNonce available in script. */
                            uint64_t  nExtraNonce;
                            tx.ssOperation >> nExtraNonce;

                            /* Ensure that it as end of tx.ssOperation. TODO: coinbase should be followed by ambassador and developer scripts */
                            if(!tx.ssOperation.end())
                                return debug::error(FUNCTION, "coinbase can't have extra data");

                            break;
                        }


                        /* Coinstake operation for trust transaction. Requires an account. */
                        case OP::TRUST:
                        {
                            /* Ensure that it as beginning of the tx.ssOperation. */
                            if(!tx.ssOperation.begin())
                                return debug::error(FUNCTION, "trust operation has to be first");

                            /* The last stake transaction for the trust account register. */
                            uint512_t hashLastTrust;
                            tx.ssOperation >> hashLastTrust;

                            /* The current calculated value for new trust score. */
                            uint64_t nTrustScore;
                            tx.ssOperation >> nTrustScore;

                            /* Coinstake reward paid to trust account by this operation. */
                            uint64_t nCoinstakeReward;
                            tx.ssOperation >> nCoinstakeReward;

                            /* Execute the operation method. */
                            if(!Trust(hashLastTrust, nTrustScore, nCoinstakeReward, nFlags, tx))
                                return false;

                            /* Ensure that it as end of tx.ssOperation. TODO: coinbase should be followed by ambassador and developer scripts */
                            if(!tx.ssOperation.end())
                                return debug::error(FUNCTION, "trust can't have extra data");

                            break;
                        }


                        /* Coinstake operation for Genesis transaction. Requires an account. */
                        case OP::GENESIS:
                        {
                            /* Ensure that it as beginning of the tx.ssOperation. */
                            if(!tx.ssOperation.begin())
                                return debug::error(FUNCTION, "genesis operation has to be first");

                            /* The register address of the trust account that is being staked. */
                            uint256_t hashAddress;
                            tx.ssOperation >> hashAddress;

                            /* Coinstake reward paid to trust account by this operation. */
                            uint64_t nCoinstakeReward;
                            tx.ssOperation >> nCoinstakeReward;

                            /* Execute the operation method. */
                            if(!Genesis(hashAddress, nCoinstakeReward, nFlags, tx))
                                return false;

                            /* Ensure that it as end of tx.ssOperation. */
                            if(!tx.ssOperation.end())
                                return debug::error(FUNCTION, "genesis can't have extra data");

                            break;
                        }


                        /* Move funds from trust account balance to stake. Requires an account. */
                        case OP::STAKE:
                        {
                            /* The register address of the trust account. */
                            uint256_t hashAddress;
                            tx.ssOperation >> hashAddress;

                            /* Amount to of funds to move. */
                            uint64_t nAmount;
                            tx.ssOperation >> nAmount;

                            /* Execute the operation method. */
                            if(!Stake(hashAddress, nAmount, nFlags, tx))
                                return false;

                            break;
                        }


                        /* Move funds from trust account stake to balance. Requires an account. */
                        case OP::UNSTAKE:
                        {
                            /* The register address of the trust account. */
                            uint256_t hashAddress;
                            tx.ssOperation >> hashAddress;

                            /* Amount of funds to move. */
                            uint64_t nAmount;
                            tx.ssOperation >> nAmount;

                            /* Trust score penalty from unstake. */
                            uint64_t nTrustPenalty;
                            tx.ssOperation >> nTrustPenalty;

                            /* Execute the operation method. */
                            if(!Unstake(hashAddress, nAmount, nTrustPenalty, nFlags, tx))
                                return false;

                            break;
                        }


                        /* Authorize a transaction to happen with a temporal proof. */
                        case OP::AUTHORIZE:
                        {
                            /* The transaction that you are authorizing. */
                            uint512_t hashTx;
                            tx.ssOperation >> hashTx;

                            /* The proof you are using that you have rights. */
                            uint256_t hashProof;
                            tx.ssOperation >> hashProof;

                            /* Execute the operation method. */
                            if(!Authorize(hashTx, hashProof, nFlags, tx))
                                return false;

                            break;
                        }


                        case OP::ACK:
                        {
                            /* The object register to tally the votes for. */
                            uint256_t hashRegister;

                            //TODO: OP::ACK -> tally trust to object register "trust" field.
                            //tally stake to object register "stake" field

                            break;
                        }


                        case OP::NACK:
                        {
                            /* The object register to tally the votes for. */
                            uint256_t hashRegister;

                            //TODO: OP::NACK -> remove trust from LLD
                            //remove stake from object register "stake" field

                            break;
                        }


                        //case OP::SIGNATURE:
                        //{
                            //a transaction proof that designates the hashOwner or genesisID has signed published data
                            //>> vchData. to prove this data was signed by being published. This can be a hash if needed.

                        //    break;
                        //}

                        default:
                            return debug::error(FUNCTION, "operations reached invalid stream state");
                    }
                }
            }
            catch(const std::runtime_error& e)
            {
                return debug::error(FUNCTION, "exception encountered ", e.what());
            }

            /* If nothing failed, return true for evaluation. */
            return true;
        }

    }
}

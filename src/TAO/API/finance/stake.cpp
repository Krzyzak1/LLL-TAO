/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLC/hash/SK.h>

#include <LLD/include/global.h>

#include <TAO/API/include/finance.h>
#include <TAO/API/include/global.h>
#include <TAO/API/include/utils.h>

#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/include/create.h>
#include <TAO/Ledger/include/stake.h>
#include <TAO/Ledger/types/mempool.h>

#include <TAO/Operation/include/enum.h>
#include <TAO/Operation/include/execute.h>

#include <TAO/Register/include/enum.h>
#include <TAO/Register/types/object.h>

/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        /* Set the stake amount for trust account (add/remove stake). */
        json::json Finance::Stake(const json::json& params, bool fHelp)
        {
            json::json ret;

            /* Get the PIN to be used for this API call */
            SecureString strPIN = users->GetPin(params);

            /* Get the session to be used for this API call */
            uint64_t nSession = users->GetSession(params);

            /* Get the user account. */
            memory::encrypted_ptr<TAO::Ledger::SignatureChain>& user = users->GetAccount(nSession);
            if(!user)
                throw APIException(-25, "Invalid session ID");

            /* Check for amount parameter. */
            if(params.find("amount") == params.end())
                throw APIException(-25, "Missing Amount. (<amount>)");

            /* Check that the account is unlocked for creating transactions */
            if(!users->CanTransact())
                throw APIException(-25, "Account has not been unlocked for transactions.");

            /* Create the transaction. */
            TAO::Ledger::Transaction tx;
            if(!TAO::Ledger::CreateTransaction(user, strPIN, tx))
                throw APIException(-25, "Failed to create transaction.");

            /* Register address is a hash of a name in the format of namespacehash:objecttype:name */
            std::string strRegisterName = NamespaceHash(user->UserName()).ToString() + ":token:trust";

            /* Build the address from an SK256 hash of register name. Need for indexed trust accounts, also, as return value */
            uint256_t hashRegister = LLC::SK256(std::vector<uint8_t>(strRegisterName.begin(), strRegisterName.end()));

            /* Get trust account. Any trust account that has completed Genesis will be indexed. */
            TAO::Register::Object trustAccount;

            if (LLD::regDB->HasTrust(user->Genesis()))
            {
                /* Trust account is indexed */
                if (!LLD::regDB->ReadTrust(user->Genesis(), trustAccount))
                   throw APIException(-24, "Unable to retrieve trust account.");

                if(!trustAccount.Parse())
                    throw APIException(-24, "Unable to parse trust account register.");
            }
            else
            {
                if(!LLD::regDB->ReadState(hashRegister, trustAccount, TAO::Register::FLAGS::MEMPOOL))
                    throw APIException(-24, "Trust account not found");

                if(!trustAccount.Parse())
                    throw APIException(-24, "Unable to parse trust account.");

                /* Check the object standard. */
                if( trustAccount.Standard() != TAO::Register::OBJECTS::TRUST)
                    throw APIException(-24, "Register is not a trust account");

                /* Check the account is a NXS account */
                if(trustAccount.get<uint256_t>("token_address") != 0)
                    throw APIException(-24, "Trust account is not a NXS account.");
            }

            uint64_t nBalancePrev = trustAccount.get<uint64_t>("balance");
            uint64_t nStakePrev = trustAccount.get<uint64_t>("stake");
            uint64_t nPendingStakePrev = trustAccount.get<uint64_t>("pending_stake");
            uint64_t nStakeTotalPrev = nStakePrev + nPendingStakePrev;

            /* Get the new total stake amount. */
            uint64_t nStakeTotal = std::stod(params["amount"].get<std::string>()) * TAO::Ledger::NXS_COIN;

            if(nStakeTotal > nStakeTotalPrev)
            {
                /* Adding to stake from balance */
                uint64_t nAddStake = nStakeTotal - nStakeTotalPrev;

                if(nAddStake > nBalancePrev)
                    throw APIException(-25, "Insufficient trust account balance to add to stake");

                tx << (uint8_t)TAO::Operation::OP::STAKE << hashRegister << nAddStake;
            }
            else if (nStakeTotal < nStakeTotalPrev)
            {
                /* Removing from stake to balance */
                uint64_t nRemoveStake = nStakeTotalPrev - nStakeTotal;

                uint64_t nTrustPenalty = 0;

                if (nRemoveStake > nPendingStakePrev)
                {
                    /* Unstaking more than pending stake amount takes remainder from stake and applies trust penalty */
                    uint64_t nStake = nStakePrev - (nRemoveStake - nPendingStakePrev);
                    uint64_t nTrustPrev = trustAccount.get<uint64_t>("trust");

                    nTrustPenalty = TAO::Ledger::GetUnstakePenalty(nTrustPrev, nStakePrev, nStake);
                }

                tx << (uint8_t)TAO::Operation::OP::UNSTAKE << hashRegister << nRemoveStake << nTrustPenalty;
            }
            else
                throw APIException(-22, "No stake change from set stake");

            /* Execute the operations layer. */
            if(!TAO::Operation::Execute(tx, TAO::Register::FLAGS::PRESTATE | TAO::Register::FLAGS::POSTSTATE))
                throw APIException(-26, "Operations failed to execute");

            /* Sign the transaction. */
            if(!tx.Sign(users->GetKey(tx.nSequence, strPIN, nSession)))
                throw APIException(-26, "Ledger failed to sign transaction");

            /* Execute the operations layer. */
            if(!TAO::Ledger::mempool.Accept(tx))
                throw APIException(-26, "Failed to accept");

            /* Build a JSON response object. */
            ret["txid"] = tx.GetHash().ToString();

            return ret;
        }
    }
}

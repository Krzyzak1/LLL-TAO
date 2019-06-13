/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/
#include <unordered_set>

#include <Legacy/include/evaluate.h>

#include <LLD/include/global.h>

#include <TAO/API/include/global.h>
#include <TAO/API/include/utils.h>
#include <TAO/API/types/exception.h>

#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/include/chainstate.h>
#include <TAO/Ledger/include/difficulty.h>
#include <TAO/Ledger/types/tritium.h>
#include <TAO/Ledger/types/mempool.h>
#include <TAO/Ledger/types/sigchain.h>

#include <TAO/Operation/types/stream.h>
#include <TAO/Operation/include/enum.h>

#include <TAO/Register/include/create.h>
#include <TAO/Register/include/names.h>

#include <Util/include/args.h>
#include <Util/include/hex.h>
#include <Util/include/json.h>
#include <Util/include/base64.h>
#include <Util/include/debug.h>

#include <LLC/hash/argon2.h>



/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {
        /* Creates a new Name Object register for the given name and register 
           address adds the register operation to the contract */
        TAO::Operation::Contract CreateNameContract(const uint256_t& hashGenesis, const std::string& strFullName,
                        const uint256_t& hashRegister)
        {
            /* Declare the contract for the response */
            TAO::Operation::Contract contract;

            /* The hash representing the namespace that the Name will be created in.  For user local this will be the genesis hash
               of the callers sig chain.  For global namespace this will be a SK256 hash of the namespace name. */
            uint256_t hashNamespace = 0;

            /* The register address of the Name object. */
            uint256_t hashNameAddress = 0;

            /* The namespace string, populated if the caller has passed the name in name.namespace format */
            std::string strNamespace = "";

            /* The name of the Name object */
            std::string strName = strFullName;

            /* Check to see whether the name has a namesace suffix */
            size_t nPos = strName.find_last_of(".");
            if(nPos != strName.npos)
            {
                /* If so then strip off the namespace so that we can check that this user has permission to use it */
                strName = strName.substr(0, nPos);
                strNamespace = strFullName.substr(nPos+1);
            
                /* Namespace hash is a SK256 hash of the namespace name */
                hashNamespace = LLC::SK256(strNamespace);

                /* Retrieve the namespace object and check that the hashGenesis is the owner */
                TAO::Register::Object namespaceObject;
                if(!TAO::Register::GetNamespaceRegister(strNamespace, namespaceObject))
                    throw APIException(-23, "Namespace does not exist: " + strNamespace);

                /* Check the owner is the hashGenesis */
                if(namespaceObject.hashOwner != hashGenesis)
                    throw APIException(-23, "Cannot create a name in namespace " + strNamespace + " as you are not the owner.");
            }
            else
            {
                /* If no global namespace suffix has been passed in then use the callers genesis hash for the hashNamespace. */
                hashNamespace = hashGenesis;
            }
            

            /* Obtain the name register address for the genesis/name combination */
            TAO::Register::GetNameAddress(hashNamespace, strName, hashNameAddress);

            /* Check to see whether the name already exists  */
            TAO::Register::Object object;
            if(LLD::Register->ReadState(hashNameAddress, object, TAO::Ledger::FLAGS::MEMPOOL))
            {
                if(!strNamespace.empty())
                    throw APIException(-23, "An object with this name already exists in this namespace.");
                else
                    throw APIException(-23, "An object with this name already exists for this user.");
            }
                

            /* Create the Name register object pointing to hashRegister */
            TAO::Register::Object name = TAO::Register::CreateName(strNamespace, strName, hashRegister);

            /* Add the Name object register operation to the transaction */
            contract << uint8_t(TAO::Operation::OP::CREATE) << hashNameAddress << uint8_t(TAO::Register::REGISTER::OBJECT) << name.GetState();
        
            return contract;
        }


        /* Creates a new Name Object register for an object being transferred */
        TAO::Operation::Contract CreateNameContractFromTransfer(const uint512_t& hashTransfer, const uint256_t& hashGenesis)
        {
            /* Declare the contract for the response */
            TAO::Operation::Contract contract;

            /* Firstly retrieve the transfer transaction that is being claimed so that we can get the address of the object */
            TAO::Ledger::Transaction txPrev;;

            /* Check disk of writing new block. */
            if(!LLD::Ledger->ReadTx(hashTransfer, txPrev, TAO::Ledger::FLAGS::MEMPOOL))
                throw APIException(-23, "Transfer transaction not found.");

            /* Ensure we are not claiming our own Transfer.  If we are then no need to create a Name object as we already have one */
            if(txPrev.hashGenesis != hashGenesis)
            {
                /* Loop through the contracts. */
                for(uint32_t nContract = 0; nContract < txPrev.Size(); ++nContract)
                {
                    /* Get a reference of the contract. */
                    const TAO::Operation::Contract& check = txPrev[nContract];

                    /* Extract the OP from tx. */
                    uint8_t OP = 0;
                    check >> OP;

                    /* Check for proper op. */
                    if(OP != TAO::Operation::OP::TRANSFER)
                        continue;

                    /* Extract the object register address  */
                    uint256_t hashAddress = 0;
                    check >> hashAddress;

                    /* Now check the previous owners Name records to see if there was a Name for this object */
                    std::string strAssetName = GetRegisterName(hashAddress, txPrev.hashGenesis, txPrev.hashGenesis);

                    /* If a name was found then create a Name record for the new owner using the same name */
                    if(!strAssetName.empty())
                        contract = CreateNameContract(hashGenesis, strAssetName, hashAddress);

                    /* If found break. */
                    break;
                }
            }

            return contract;
        }


        /* Updates the register address of a Name object */
        void UpdateName(const uint256_t& hashGenesis, const std::string strName,
                        const uint256_t& hashRegister, TAO::Operation::Contract& contract)
        {
            uint256_t hashNameAddress;

            /* Obtain a new name register address for the updated genesis/name combination */
            TAO::Register::GetNameAddress(hashGenesis, strName, hashNameAddress);

            /* Check to see whether the name already. */
            TAO::Register::Object object;
            if(LLD::Register->ReadState(hashNameAddress, object, TAO::Ledger::FLAGS::MEMPOOL))
                throw APIException(-23, "An object with this name already exists for this user.");

           /* TODO */
        }


        /* Resolves a register address from a name by looking up the Name object. */
        uint256_t AddressFromName(const json::json& params, const std::string& strObjectName)
        {
            uint256_t hashRegister = 0;

            /* In order to resolve an object name to a register address we also need to know the namespace.
            *  This must either be provided by the caller explicitly in a namespace parameter or by passing
            *  the name in the format namespace:name.  However since the default namespace is the username
            *  of the sig chain that created the object, if no namespace is explicitly provided we will
            *  also try using the username of currently logged in sig chain */
            std::string strName = strObjectName;
            std::string strNamespace = "";

            /* Declare the namespace hash to use for this object. */
            uint256_t hashNamespace = 0;

            /* First check to see if the name parameter has been provided in either the userspace:name  or name.namespace format*/
            size_t nNamespacePos = strName.find(".");
            size_t nUserspacePos = strName.find(":");
            
            if(nNamespacePos != std::string::npos)
            {
                strNamespace = strName.substr(nNamespacePos+1);
                strName = strName.substr(0, nNamespacePos);

                /* Namespace hash is a SK256 hash of the namespace name */
                hashNamespace = LLC::SK256(strNamespace);
            }
            
            else if(nUserspacePos != std::string::npos)
            {
                strNamespace = strName.substr(0, nUserspacePos);
                strName = strName.substr(nUserspacePos+1);

                /* get the namespace hash from the namespace name */
                hashNamespace = TAO::Ledger::SignatureChain::Genesis(SecureString(strNamespace.c_str()));
            }

            /* If neither of those then check to see if there is an active session to access the sig chain */
            else
            {
                /* Get the session to be used for this API call.  Note we pass in false for fThrow here so that we can
                   throw a missing namespace exception if no valid session could be found */
                uint64_t nSession = users->GetSession(params, false);

                /* Get the account. */
                memory::encrypted_ptr<TAO::Ledger::SignatureChain>& user = users->GetAccount(nSession);
                if(!user)
                    throw APIException(-23, "Missing namespace parameter");

                hashNamespace = user->Genesis();
            }

            /* Read the Name Object */
            TAO::Register::Object object;
            if(!TAO::Register::GetNameRegister(hashNamespace, strName, object))
            {
                if(strNamespace.empty())
                    throw APIException(-24, debug::safe_printstr( "Unknown name: ", strName));
                else
                    throw APIException(-24, debug::safe_printstr( "Unknown name: ", strNamespace, ":", strName));
            }

            /* Get the address that this name register is pointing to */
            hashRegister = object.get<uint256_t>("address");

            return hashRegister;
        }


        /* Determins whether a string value is a register address.
        *  This only checks to see if the value is 64 characters in length and all hex characters (i.e. can be converted to a uint256).
        *  It does not check to see whether the register address exists in the database
        */
        bool IsRegisterAddress(const std::string& strValueToCheck)
        {
            return strValueToCheck.length() == 64 && strValueToCheck.find_first_not_of("0123456789abcdefABCDEF", 0) == std::string::npos;
        }


        /* Retrieves the number of digits that applies to amounts for this token or account object.
        *  If the object register passed in is a token account then we need to look at the token definition
        *  in order to get the digits.  The token is obtained by looking at the identifier field,
        *  which contains the register address of the issuing token
        */
        uint64_t GetDigits(const TAO::Register::Object& object)
        {
            /* Declare the nDigits to return */
            uint64_t nDigits = 0;

            /* Get the object standard. */
            uint8_t nStandard = object.Standard();

            /* Check the object standard. */
            switch(nStandard)
            {
                case TAO::Register::OBJECTS::TOKEN:
                {
                    nDigits = object.get<uint64_t>("digits");
                    break;
                }

                case TAO::Register::OBJECTS::TRUST:
                {
                    nDigits = TAO::Ledger::NXS_DIGITS; // NXS token default digits
                    break;
                }

                case TAO::Register::OBJECTS::ACCOUNT:
                {
                    /* If debiting an account we need to look at the token definition in order to get the digits. */
                    uint256_t nIdentifier = object.get<uint256_t>("token");

                    /* Edge case for NXS token which has identifier 0, so no look up needed */
                    if(nIdentifier == 0)
                        nDigits = TAO::Ledger::NXS_DIGITS;
                    else
                    {

                        TAO::Register::Object token;
                        if(!LLD::Register->ReadState(nIdentifier, token, TAO::Ledger::FLAGS::MEMPOOL))
                            throw APIException(-24, "Token not found");

                        /* Parse the object register. */
                        if(!token.Parse())
                            throw APIException(-24, "Object failed to parse");

                        nDigits = token.get<uint64_t>("digits");
                    }
                    break;
                }

                default:
                {
                    throw APIException(-27, "Unknown token / account.");
                }

            }

            return nDigits;
        }


        /* Retrieves the token name for the token that this account object is used for.
        *  The token is obtained by looking at the token_address field,
        *  which contains the register address of the issuing token */
        std::string GetTokenNameForAccount(const uint256_t& hashCaller, const TAO::Register::Object& object)
        {
            /* Declare token name to return  */
            std::string strTokenName;

            /* Get the object standard. */
            uint8_t nStandard = object.Standard();

            /* Check the object register standard. */
            if(nStandard == TAO::Register::OBJECTS::ACCOUNT)
            {
                /* If debiting an account we need to look at the token definition in order to get the digits.
                   The token is obtained by looking at the token_address field, which contains the register address of
                   the issuing token */
                uint256_t nIdentifier = object.get<uint256_t>("token");

                /* Edge case for NXS token which has identifier 0, so no look up needed */
                if(nIdentifier == 0)
                    strTokenName = "NXS";
                else
                {

                    TAO::Register::Object token;
                    if(!LLD::Register->ReadState(nIdentifier, token, TAO::Ledger::FLAGS::MEMPOOL))
                        throw APIException(-24, "Token not found");

                    /* Parse the object register. */
                    if(!token.Parse())
                        throw APIException(-24, "Object failed to parse");

                    /* Look up the token name based on the Name records in thecaller's sig chain */
                    strTokenName = GetRegisterName(nIdentifier, hashCaller, token.hashOwner);

                }
            }
            else
                throw APIException(-27, "Object is not an account.");

            return strTokenName;
        }


        /* In order to work out which registers are currently owned by a particular sig chain
         * we must iterate through all of the transactions for the sig chain and track the history
         * of each register.  By iterating through the transactions from most recent backwards we
         * can make some assumptions about the current owned state.  For example if we find a debit
         * transaction for a register before finding a transfer then we must know we currently own it.
         * Similarly if we find a transfer transaction for a register before any other transaction
         * then we must know we currently to NOT own it.
         */
        bool ListRegisters(const uint256_t& hashGenesis, std::vector<uint256_t>& vRegisters)
        {
            /* Get the last transaction. */
            uint512_t hashLast = 0;

            /* Get the last transaction for this genesis.  NOTE that we include the mempool here as there may be registers that
               have been created recently but not yet included in a block*/
            if(!LLD::Ledger->ReadLast(hashGenesis, hashLast, TAO::Ledger::FLAGS::MEMPOOL))
                return false;

            /* Keep a running list of owned and transferred registers. We use a set to store these registers because
             * we are going to be checking them frequently to see if a hash is already in the container,
             * and a set offers us near linear search time
             */
            std::unordered_set<uint256_t> vTransferred;
            std::unordered_set<uint256_t> vOwnedRegisters;

            /* Loop until genesis. */
            while(hashLast != 0)
            {
                /* Get the transaction from disk. */
                TAO::Ledger::Transaction tx;
                if(!LLD::Ledger->ReadTx(hashLast, tx, TAO::Ledger::FLAGS::MEMPOOL))
                    throw APIException(-28, "Failed to read transaction");

                /* Set the next last. */
                hashLast = tx.hashPrevTx;

                /* Iterate through all contracts. */
                for(uint32_t nContract = 0; nContract < tx.Size(); ++nContract)
                {
                    /* Get the contract output. */
                    const TAO::Operation::Contract& contract = tx[nContract];

                    /* Seek to start of the operation stream in case this a transaction from mempool that has already been read*/
                    contract.Reset(TAO::Operation::Contract::OPERATIONS);

                    /* Deserialize the OP. */
                    uint8_t nOP = 0;
                    contract >> nOP;

                    /* Check the current opcode. */
                    switch(nOP)
                    {
                        /* Condition that allows a validation to occur. */
                        case TAO::Operation::OP::CONDITION:
                        {
                            /* Condition has no parameters. */
                            contract >> nOP;

                            break;
                        }


                        /* Validate a previous contract's conditions */
                        case TAO::Operation::OP::VALIDATE:
                        {
                            /* Skip over validate. */
                            contract.Seek(68);

                            /* Get next OP. */
                            contract >> nOP;

                            break;
                        }
                    }

                    /* Check the current opcode. */
                    switch(nOP)
                    {

                        /* These are the register-based operations that prove ownership if encountered before a transfer*/
                        case TAO::Operation::OP::WRITE:
                        case TAO::Operation::OP::APPEND:
                        case TAO::Operation::OP::CREATE:
                        case TAO::Operation::OP::DEBIT:
                        {
                            /* Extract the address from the contract. */
                            uint256_t hashAddress = 0;
                            contract >> hashAddress;

                            /* for these operations, if the address is NOT in the transferred list
                               then we know that we must currently own this register */
                           if(vTransferred.find(hashAddress)    == vTransferred.end()
                           && vOwnedRegisters.find(hashAddress) == vOwnedRegisters.end())
                           {
                               /* Add to owned set. */
                               vOwnedRegisters.insert(hashAddress);

                               /* Add to return vector. */
                               vRegisters.push_back(hashAddress);
                           }

                            break;
                        }


                        /* Check for credits here. */
                        case TAO::Operation::OP::CREDIT:
                        {
                            /* Seek past irrelevant data. */
                            contract.Seek(68);

                            /* The account that is being credited. */
                            uint256_t hashAddress = 0;
                            contract >> hashAddress;

                            /* If we find a credit before a transfer transaction for this register then
                               we can know for certain that we must own it */
                           if(vTransferred.find(hashAddress)    == vTransferred.end()
                           && vOwnedRegisters.find(hashAddress) == vOwnedRegisters.end())
                           {
                               /* Add to owned set. */
                               vOwnedRegisters.insert(hashAddress);

                               /* Add to return vector. */
                               vRegisters.push_back(hashAddress);
                           }

                            break;

                        }


                        /* Check for a transfer here. */
                        case TAO::Operation::OP::TRANSFER:
                        {
                            /* Extract the address from the contract. */
                            uint256_t hashAddress = 0;
                            contract >> hashAddress;

                            /* Read the register transfer recipient. */
                            uint256_t hashTransfer = 0;
                            contract >> hashTransfer;

                            /* Read the force transfer flag */
                            uint8_t nType = 0;
                            contract >> nType;

                            /* If we have transferred to a token that we own then we ignore the transfer as we still
                               technically own the register */
                            if(nType == TAO::Operation::TRANSFER::FORCE)
                            {
                                TAO::Register::Object newOwner;
                                if(!LLD::Register->ReadState(hashTransfer, newOwner))
                                    throw APIException(-24, "Transfer recipient object not found");

                                if(newOwner.hashOwner == hashGenesis)
                                    break;
                            }

                            /* If we find a TRANSFER then we can know for certain that we no longer own it */
                            if(vTransferred.find(hashAddress)    == vTransferred.end())
                                vTransferred.insert(hashAddress);

                            break;
                        }


                        /* Check for claims here. */
                        case TAO::Operation::OP::CLAIM:
                        {
                            /* Seek past irrelevant data. */
                            contract.Seek(68);

                            /* Extract the address from the contract. */
                            uint256_t hashAddress = 0;
                            contract >> hashAddress;

                            /* If we find a CLAIM transaction before a TRANSFER, then we know that we must currently own this register */
                            if(vTransferred.find(hashAddress)    == vTransferred.end()
                            && vOwnedRegisters.find(hashAddress) == vOwnedRegisters.end())
                            {
                                /* Add to owned set. */
                                vOwnedRegisters.insert(hashAddress);

                                /* Add to return vector. */
                                vRegisters.push_back(hashAddress);
                            }

                            break;
                        }

                        default:
                            continue;
                    }
                }
            }

            return true;
        }


        /* Scans the Name records associated with the hashCaller sig chain to find an entry with a matching hashObject address */
        std::string GetRegisterName(const uint256_t& hashObject, const uint256_t& hashCaller, const uint256_t& hashOwner)
        {
            /* Declare the return val */
            std::string strName = "";

            /* If the owner of the object is not the caller, then check to see whether the owner is another object owned by
               the caller.  This would be the case for a tokenized asset */
            uint256_t hashOwnerOwner = 0;
            if(hashCaller != hashOwner)
            {
                TAO::Register::Object owner;
                if(LLD::Register->ReadState(hashOwner, owner, TAO::Ledger::FLAGS::MEMPOOL))
                   hashOwnerOwner = owner.hashOwner;
            }

            /* If the caller is the object owner then attempt to find a Name record to look up the Name of this object */
            if(hashCaller == hashOwner || hashCaller == hashOwnerOwner)
            {
                /* Firstly get all object registers owned by this sig chain */
                std::vector<uint256_t> vRegisters;
                if(!ListRegisters(hashCaller, vRegisters))
                    throw APIException(-24, "No registers found");

                /* Iterate through these to find all Name registers */
                for(const auto& hashRegister : vRegisters)
                {
                    /* Get the object from the register DB.  We can read it as an Object and then check its nType
                    to determine whether or not it is a Name. */
                    TAO::Register::Object object;
                    if(!LLD::Register->ReadState(hashRegister, object, TAO::Ledger::FLAGS::MEMPOOL))
                        continue;

                    if(object.nType == TAO::Register::REGISTER::OBJECT)
                    {
                        /* parse object so that the data fields can be accessed */
                        if(!object.Parse())
                            throw APIException(-24, "Failed to parse object register");

                        /* Check the object register standards. */
                        if(object.Standard() != TAO::Register::OBJECTS::NAME)
                            continue;

                        /* Check to see whether the address stored in this Name matches the object being conveted to JSON */
                        if(object.get<uint256_t>("address") == hashObject)
                        {
                            /* Get the name from the Name register and break out since we have a match */
                            strName = object.get<std::string>("name");
                            break;
                        }
                    }
                }
            }

            return strName;
        }
    }
}

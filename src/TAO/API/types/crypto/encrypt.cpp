/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLD/include/global.h>

#include <LLC/include/encrypt.h>
#include <LLC/include/flkey.h>

#include <TAO/API/types/objects.h>
#include <TAO/API/include/global.h>
#include <TAO/API/include/utils.h>
#include <TAO/API/include/json.h>

#include <TAO/Ledger/types/mempool.h>

#include <TAO/Operation/include/enum.h>

#include <TAO/Register/types/object.h>

#include <Util/include/debug.h>
#include <Util/include/encoding.h>
#include <Util/include/base64.h>


/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        /* Generates private key based on keyname/user/pass/pin and stores it in the keyname slot in the crypto register. */
        json::json Crypto::Encrypt(const json::json& params, bool fHelp)
        {
            /* JSON return value. */
            json::json ret;

            /* The data to be encrypted */
            std::vector<uint8_t> vchData;

            /* The symmetric key used to encrypt */
            std::vector<uint8_t> vchKey;

            /* Check the caller included the data */
            if(params.find("data") == params.end() || params["data"].get<std::string>().empty())
                throw APIException(-18, "Missing data.");

            /* Decode the data into a vector of bytes */
            try
            {
                vchData = encoding::DecodeBase64(params["data"].get<std::string>().c_str());
            }
            catch(const std::exception& e)
            {
                throw APIException(-27, "Malformed base64 encoding.");
            }

            /* Check for explicit key */
            if(params.find("key") != params.end())
            {
                /* Decode the key into a vector of bytes */
                try
                {
                    vchKey = encoding::DecodeBase64(params["key"].get<std::string>().c_str());
                }
                catch(const std::exception& e)
                {
                    throw APIException(-27, "Malformed base64 encoding.");
                }
            }
            else
            {
                /* Get the PIN to be used for this API call */
                SecureString strPIN = users->GetPin(params, TAO::Ledger::PinUnlock::TRANSACTIONS);

                /* Get the session to be used for this API call */
                uint256_t nSession = users->GetSession(params);

                /* Get the account. */
                memory::encrypted_ptr<TAO::Ledger::SignatureChain>& user = users->GetAccount(nSession);
                if(!user)
                    throw APIException(-10, "Invalid session ID");

                /* Check the caller included the key name */
                if(params.find("name") == params.end() || params["name"].get<std::string>().empty())
                    throw APIException(-88, "Missing name.");
                
                /* Get the requested key name */
                std::string strName = params["name"].get<std::string>();

                /* The logged in sig chain genesis hash */
                uint256_t hashGenesis = user->Genesis();

                /* The address of the crypto object register, which is deterministic based on the genesis */
                TAO::Register::Address hashCrypto = TAO::Register::Address(std::string("crypto"), hashGenesis, TAO::Register::Address::CRYPTO);
                
                /* Read the crypto object register */
                TAO::Register::Object crypto;
                if(!LLD::Register->ReadState(hashCrypto, crypto, TAO::Ledger::FLAGS::MEMPOOL))
                    throw APIException(-259, "Could not read crypto object register");

                /* Parse the object. */
                if(!crypto.Parse())
                    throw APIException(-36, "Failed to parse object register");
                
                /* Check to see if the key name is valid */
                if(!crypto.CheckName(strName))
                    throw APIException(-260, "Invalid key name");

                /* Get the public key hash from the register */
                uint256_t hashKey = crypto.get<uint256_t>(strName);
    
                /* Check to see if the the has been generated.  Even though the key is deterministic,  */
                if(hashKey == 0)
                    throw APIException(-264, "Key not yet created");

                /* Get the key type */
                uint8_t nScheme = hashKey.GetType();

                /* Get the last transaction. */
                uint512_t hashLast;
                if(!LLD::Ledger->ReadLast(hashGenesis, hashLast, TAO::Ledger::FLAGS::MEMPOOL))
                    throw APIException(-138, "No previous transaction found");

                /* Get previous transaction */
                TAO::Ledger::Transaction txPrev;
                if(!LLD::Ledger->ReadTx(hashLast, txPrev, TAO::Ledger::FLAGS::MEMPOOL))
                    throw APIException(-138, "No previous transaction found");

                /* Get the private key. */
                uint512_t hashSecret = user->Generate(strName, 0, strPIN);

                /* Generate a new transaction hash next using the current credentials so that we can verify them. */
                TAO::Ledger::Transaction tx;
                tx.NextHash(user->Generate(txPrev.nSequence + 1, strPIN, false), txPrev.nNextType);

                /* Check the calculated next hash matches the one on the last transaction in the sig chain. */
                if(txPrev.hashNext != tx.hashNext)
                    throw APIException(-139, "Invalid credentials");

                /* If a peer key has been provided then generate a shared key */
                if(params.find("peerkey") != params.end() && !params["peerkey"].get<std::string>().empty())
                {
                    /* Check that the local key is a brainpool key as only EC keys can be used in ECDH shared keys*/
                    if(nScheme != TAO::Ledger::SIGNATURE::BRAINPOOL)
                        throw APIException(-267, "Shared key encryption only supported for EC (Brainpool) keys");
                        
                    /* Decode the public key into a vector of bytes */
                    std::vector<uint8_t> vchPubKey;
                    if(!encoding::DecodeBase58(params["peerkey"].get<std::string>(), vchPubKey))
                        throw APIException(-266, "Malformed public key.");

                    /* Get the secret from new key. */
                    std::vector<uint8_t> vBytes = hashSecret.GetBytes();
                    LLC::CSecret vchSecret(vBytes.begin(), vBytes.end());

                    /* Create an ECKey for the private key */
                    LLC::ECKey keyPrivate = LLC::ECKey(LLC::BRAINPOOL_P512_T1, 64);

                    /* Set the secret key. */
                    if(!keyPrivate.SetSecret(vchSecret, true))
                        throw APIException(269, "Malformed private key");

                    /* Create an ECKey for the public key */
                    LLC::ECKey keyPublic = LLC::ECKey(LLC::BRAINPOOL_P512_T1, 64);
                    if(!keyPublic.SetPubKey(vchPubKey))
                        throw APIException(-266, "Malformed public key.");

                    /* Generate the shared symmetric key */
                    if(!LLC::ECKey::MakeShared(keyPrivate, keyPublic, vchKey))
                        throw APIException(268, "Failed to generate shared key");

                    /* Add the public key for our private keyto the response */
                    ret["publickey"] = encoding::EncodeBase58(keyPrivate.GetPubKey());

                }
                else
                {
                    /* Otherwise we use the private key as the symmetric key */
                    vchKey = hashSecret.GetBytes();

                    /* Get the secret from key. */
                    LLC::CSecret vchSecret(vchKey.begin(), vchKey.end());

                    /* Get the public key to populate into the response */
                    if(nScheme == TAO::Ledger::SIGNATURE::BRAINPOOL)
                    {
                        /* Create an ECKey for the private key */
                        LLC::ECKey keyPrivate = LLC::ECKey(LLC::BRAINPOOL_P512_T1, 64);

                        /* Set the secret key. */
                        if(!keyPrivate.SetSecret(vchSecret, true))
                            throw APIException(269, "Malformed private key");

                        /* Add the public key to the response */
                        ret["publickey"] = encoding::EncodeBase58(keyPrivate.GetPubKey());
                    }
                    else if(nScheme == TAO::Ledger::SIGNATURE::FALCON)
                    {
                        /* Create an FLKey for the private key */
                        LLC::FLKey keyPrivate = LLC::FLKey();

                        /* Set the secret key. */
                        if(!keyPrivate.SetSecret(vchSecret))
                            throw APIException(269, "Malformed private key");

                        /* Add the public key to the response */
                        ret["publickey"] = encoding::EncodeBase58(keyPrivate.GetPubKey());
                    }
                }
                
                /* add the hash key */
                ret["hashkey"] = encoding::EncodeBase58(hashKey.GetBytes());

            }

            /* The encrypted data */
            std::vector<uint8_t> vchCipherText;

            /* Encrypt the data */
            if(!LLC::EncryptAES256(vchKey, vchData, vchCipherText))
                throw APIException(-270, "Failed to encrypt data.");

            /* Add the ciphertext to the response, base64 encoded*/
            ret["data"] = encoding::EncodeBase64(&vchCipherText[0], vchCipherText.size());


            return ret;
        }
    }
}

/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLD/include/global.h>

#include <LLP/include/global.h>
#include <LLP/types/tritium.h>

#include <TAO/API/users/types/users.h>
#include <TAO/API/types/sessionmanager.h>

#include <Util/include/hex.h>

/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        /* Login to a user account. */
        json::json Users::Logout(const json::json& params, bool fHelp)
        {
            /* JSON return value. */
            json::json ret;

            /* Check for username parameter. */
            if(config::fMultiuser.load() && params.find("session") == params.end())
                throw APIException(-12, "Missing Session ID");

            /* For sessionless API use the active sig chain which is stored in session 0 */
            uint256_t nSession = 0;
            if(config::fMultiuser.load())
                nSession.SetHex(params["session"].get<std::string>());

            if(!GetSessionManager().Has(nSession))
                throw APIException(-141, "Already logged out");

            /* Gracefully terminate the session */
            TerminateSession(nSession);

            ret["success"] = true;
            return ret;
        }
    }
}

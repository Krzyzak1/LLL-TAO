/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#pragma once
#ifndef NEXUS_TAO_REGISTER_INCLUDE_RESERVED_H
#define NEXUS_TAO_REGISTER_INCLUDE_RESERVED_H

/* Global TAO namespace. */
namespace TAO
{

    /* Register Layer namespace. */
    namespace Register
    {

        /** Hold list of reserved object register values. **/
        const std::vector<std::string> RESERVED =
        {
            /* Used to hold token blance. */
            "balance",

            /* Used to identify a specific token. */
            "token_address",

            /* Used to set validation script in object register. */
            "require",

            /* Used for token domain parameters for max supply. */
            "supply",

            /* Used to determine digits of a token. */
            "digits",

            /* Used to determine collective trust in a register. */
            "trust",

            /* Used to determine locked stake for a register. */
            "stake",

            /* Used to determine non-locked stake for a register. */
            "pending_stake",

            /* Used for internal object register system memory. */
            "system",

            /* Used for object register names. Reserved to never be changed. */
            "name"
        };


        /** Reserved
         *
         *  Object register keys that are reserved
         *
         *  @param[in] strValue The value to check.
         *
         *  @return True if value is object register reserved value.
         *
         **/
        inline bool Reserved(const std::string& strValue)
        {
            return std::find(RESERVED.begin(), RESERVED.end(), strValue) != RESERVED.end();
        }
    }
}

#endif

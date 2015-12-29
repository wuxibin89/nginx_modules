// Uri encode and decode.
// RFC1630, RFC1738, RFC2396

#ifndef __URI_CODEC_H__
#define __URI_CODEC_H__

#include <string>
#include <assert.h>

const char HEX2DEC[256] = {
    /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
    /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
    
    /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    
    /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    
    /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};
 
/*
 * RFC3986 unreserved characters: 0-9 a-z A-Z -._~
 */
const char SAFE[256] = {
    /*      0 1 2 3  4 5 6 7  8 9 A B  C D E F */
    /* 0 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* 1 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* 2 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,1,0,
    /* 3 */ 1,1,1,1, 1,1,1,1, 1,1,0,0, 0,0,0,0,
    
    /* 4 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
    /* 5 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,1,
    /* 6 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
    /* 7 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,1,0,
    
    /* 8 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* 9 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* A */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* B */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    
    /* C */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* D */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* E */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* F */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};

   
std::string UriDecode(const std::string &uri) {
    // Note from RFC1630:  "Sequences which start with a percent sign
    // but are not followed by two hexadecimal characters (0-9, A-F) are reserved
    // for future extension"
    
    const unsigned char *puri = (const unsigned char *)uri.c_str();
    const unsigned char *const SRC_END = puri + uri.length();
    const unsigned char *const SRC_LAST_DEC = SRC_END - 2;   // last decodable '%' 

    std::string result;
    result.reserve(uri.size());

    while (puri < SRC_LAST_DEC) {
	if (*puri == '%') {
            char dec1, dec2;

            dec1 = HEX2DEC[*(puri + 1)];
            dec2 = HEX2DEC[*(puri + 2)];

            if(dec1 != -1 && dec2 != -1) {
                result.append(1, (dec1 << 4) + dec2);
                puri += 3;
                continue;
            }
        } 
        
        if(*puri == '+') {              //application/x-www-form-urlencoded
            result.append(1, ' ');
            puri++;
            continue;
        }

        result.append(1, *puri++);
    }

    // the last 2 characters
    while (puri < SRC_END)
        result.append(1, *puri++);

    return result;
}


std::string UriEncode(const std::string &uri) {
    const char DEC2HEX[16 + 1] = "0123456789ABCDEF";
    const unsigned char * puri = (const unsigned char *)uri.c_str();
    const unsigned char * const SRC_END = puri + uri.length();

    std::string result;

    for (; puri < SRC_END; ++puri) {
        if (SAFE[*puri]) {
            result.append(1, *puri);
        } else {
            // escape this char
            result.append(1, '%');
            result.append(1, DEC2HEX[*puri >> 4]);
            result.append(1, DEC2HEX[*puri & 0x0F]);
        }
    }

    return result;
}

#endif

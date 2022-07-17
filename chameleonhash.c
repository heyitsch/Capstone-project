#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <math.h>
#include <openssl/ssl.h>
#include <openssl/ecdsa.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/hmac.h>
#include <openssl/engine.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <curl/curl.h>
#include <time.h>

// BIO
BIO *outbio = NULL;

// EC Curve
EC_GROUP *ecCurve = NULL;

// BIGNUM Variables
BIGNUM *bnOrder = NULL;
BN_CTX *ctx = NULL;

// Trapdoor is private key
BIGNUM *trapdoor = NULL;

// Random
BIGNUM *bnRandom = NULL;

// EC Points
EC_POINT *public_key = NULL;
EC_POINT *chameleonHash = NULL;       // K0
EC_POINT *modifyChameleonHash = NULL; // K1

// Seed is r' that generates the hash
BIGNUM *seed = NULL; // S1

// Public and private key
char *hex_public_key = "04B24109EA9DADAC32AC777F28E26A2449E5C48407EE88B06502A9F94FF48B1FEE9CACD9B63C3FFB80DB28C86A5C18A8523AD63B64EFD4A6E747F9FE0C6CFF3035";
char *hex_private_key = "32A8B5DECAADC1C7C51C17EBC3D89646D65D6130BED7BC2B8CBF381DE8315C65";

unsigned char *msg = NULL;
unsigned char *modifyMsg = NULL;

// Generate 512bit random prime number
BIGNUM *generateRandomPrime()
{
    BIGNUM *num = NULL;
    if ((num = BN_new()) == NULL)
        return NULL;

    BN_generate_prime_ex(num, 520, 0, NULL, NULL, NULL);

    // BN_print_fp(stdout, num);
    // printf("\n");
    return num;
}

/* Compute Chameleon Hash, [HMAC(m,Y)]Y + rG, where message m, public key Y and random number r
 * return EC_POINT * of Chameleon Hash
 */
EC_POINT *generateChameleonHash(EC_GROUP *E, EC_POINT *Y, unsigned char *m, BIGNUM *r)
{
    int key_len;
    unsigned int bytes;

    unsigned char hmac_digest[SHA256_DIGEST_LENGTH];
    unsigned char hmac_key[128];

    EC_POINT *rG = EC_POINT_new(E);
    EC_POINT *hmac_mY_Y = EC_POINT_new(E);
    EC_POINT *HK = EC_POINT_new(E);

    BIGNUM *bn_hmac_mY = BN_new();

    // Convert the EC Points Y into binary form to be used as the HMAC key "hmac_key"
    key_len = EC_POINT_point2oct(E, Y, POINT_CONVERSION_UNCOMPRESSED, hmac_key, sizeof(hmac_key), ctx);
    // Compute HMAC(m,Y)
    HMAC(EVP_sha256(), hmac_key, key_len, m, strlen((char *)m), hmac_digest, &bytes);
    // Convert the output from HMAC(m,Y) into a BIGNUM format
    BN_bin2bn(hmac_digest, bytes, bn_hmac_mY);
    // Compute scalar multiplication, rG
    EC_POINT_mul(E, rG, r, NULL, NULL, ctx);
    // Compute Y[HMAC(m,Y)]
    EC_POINT_mul(E, hmac_mY_Y, NULL, Y, bn_hmac_mY, ctx);
    // This is the Chameleon hash result [HMAC(m,Y)]Y + rG
    EC_POINT_add(E, HK, hmac_mY_Y, rG, ctx);

    EC_POINT_free(rG);
    EC_POINT_free(hmac_mY_Y);
    BN_free(bn_hmac_mY);

    return HK;
}

//  to find r0' take in *m = m0 orginal version of V0  , *m_prime = k1 || v1 
BIGNUM *computeChameleonRPrime(EC_GROUP *E, EC_POINT *Y, BIGNUM *y, BIGNUM *r, unsigned char *m, unsigned char *m_prime)
{
    int key_len;
    unsigned int hmac_length;
    unsigned int hmac_length2;

    unsigned char hmac_digest[SHA256_DIGEST_LENGTH];
    unsigned char hmac_digest2[SHA256_DIGEST_LENGTH];
    unsigned char hmac_key[128];

    BIGNUM *bn_hmac_mY_1 = NULL;
    BIGNUM *bn_hmac_mY_2 = NULL;
    BIGNUM *bnTempResult1 = BN_new();
    BIGNUM *bnTempResult2 = BN_new();
    BIGNUM *bn_r_prime = BN_new();

    // Convert the EC Points Y into binary form to be used as the HMAC key "hmac_key"
    key_len = EC_POINT_point2oct(ecCurve, Y, POINT_CONVERSION_UNCOMPRESSED, hmac_key, sizeof(hmac_key), ctx);
    // Compute HMAC(m,Y)
    HMAC(EVP_sha256(), hmac_key, key_len, m, strlen((char *)m), hmac_digest, &hmac_length);
    // Convert the output from HMAC(m,Y) into a BIGNUM format
    bn_hmac_mY_1 = BN_bin2bn(hmac_digest, hmac_length, NULL);
    // Compute HMAC(m',Y)
    HMAC(EVP_sha256(), hmac_key, key_len, m_prime, strlen((char *)m_prime), hmac_digest2, &hmac_length2);
    // Convert the output from HMAC(m',Y) into a BIGNUM format
    bn_hmac_mY_2 = BN_bin2bn(hmac_digest2, hmac_length2, NULL);
    // Compute r_1'
    // Compute HMAC(m,Y) - HMAC(m',Y)
    BN_sub(bnTempResult1, bn_hmac_mY_1, bn_hmac_mY_2);
    // Compute y[HMAC(m,Y) - HMAC(m',Y)]
    BN_mul(bnTempResult2, bnTempResult1, y, ctx);
    // r_2' = r
    // Compute r' = y[HMAC(m,Y) - HMAC(m',Y)] + r
    BN_add(bn_r_prime, bnTempResult2, r);

    BN_free(bn_hmac_mY_1);
    BN_free(bn_hmac_mY_2);
    BN_free(bnTempResult1);
    BN_free(bnTempResult2);

    return bn_r_prime;
}

void init()
{
    //  Initialize openssl function calls
    OpenSSL_add_all_algorithms();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();

    // Create the Input/Output BIO's.
    outbio = BIO_new_fp(stdout, BIO_NOCLOSE);

    // EC Cruve parameters
    if ((ctx = BN_CTX_new()) == NULL)
        return;
    if ((bnOrder = BN_new()) == NULL)
        return;

    // Generate the curve belonging to NIST Prime-Curve P-256
    ecCurve = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);

    // AN optimization for point computation
    EC_GROUP_precompute_mult(ecCurve, ctx);
}

void generateECKeys()
{
    EC_KEY *ecKey = EC_KEY_new();
    EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
    EC_KEY_set_group(ecKey, group);

    EC_KEY_generate_key(ecKey);
    // EC_KEY_print(outbio, ecKey, 1);

    BIGNUM *pPrivKey = NULL;
    EC_POINT *pPubKey = NULL;

    pPrivKey = (BIGNUM *)EC_KEY_get0_private_key(ecKey);
    hex_private_key = BN_bn2hex(pPrivKey);

    pPubKey = (EC_POINT *)EC_KEY_get0_public_key(ecKey);
    hex_public_key = EC_POINT_point2hex(ecCurve, pPubKey, POINT_CONVERSION_UNCOMPRESSED, ctx);

    EC_KEY_free(ecKey);
    EC_GROUP_free(group);
}

void setPrivatePublicKeyECPoints()
{
    // printf("Pukey: %s\n Prkey: %s\n", hex_public_key, hex_private_key);

    // Create EC Point from hex of public key
    public_key = EC_POINT_new(ecCurve);
    EC_POINT_hex2point(ecCurve, hex_public_key, public_key, ctx);

    // Create BIGNUM for private key
    trapdoor = BN_new();
    BN_hex2bn(&trapdoor, hex_private_key);
}

void setMessage(bool type, char *message)
{
    if (type)
    {
        msg = (unsigned char *)message;
    }
    else
    {
        modifyMsg = (unsigned char *)message;
    }
}

int main(int argc, char *argv[])
{

    init();
    // generateECKeys();
    setPrivatePublicKeyECPoints();

    if (argc > 1)
    {
        switch (atoi(argv[1]))
        {
        case 0:
        {
            // Generate chameleon hash for the file data
            // Less than 3 or more than 3 arguments = Fail
            // create a key using a message string
            if (argc < 3 || argc > 3)
            {
                printf("Fail\n");
                break;
            }

            BIGNUM *rand = generateRandomPrime();
                        // generateChameleonHash(EC_GROUP *E, EC_POINT *Y, unsigned char *m, BIGNUM *r)
            chameleonHash = generateChameleonHash(ecCurve, public_key, (unsigned char *)argv[2], rand);

            char *hexRand = BN_bn2hex(rand);
            char *hexCH = EC_POINT_point2hex(ecCurve, chameleonHash, POINT_CONVERSION_UNCOMPRESSED, ctx);

            printf("%s,%s\n", hexCH, hexRand);

            free(hexRand);
            free(hexCH);
            BN_free(rand);
            EC_POINT_free(chameleonHash);

            break;
        }
        case 1:
        {
            // Calculate the collision r' of two input keys
            // Generate R' for K0=CH(K1,R')
            if (argc < 5 || argc > 5)
            {
                printf("Fail\n");
                break;
            }

            // K0's random number (from case 0 above)
            BIGNUM *random = NULL;
            BN_hex2bn(&random, argv[2]);
            // argv[3] = navbits  original message (from case 0 above), argv[4] = K1

                             // r' = Y, y, K1||V1 , m0. r0
                            // computeChameleonRPrime(EC_GROUP *E, EC_POINT *Y, BIGNUM *y, BIGNUM *r, unsigned char *m, unsigned char *m_prime)
            BIGNUM *randomPrime = computeChameleonRPrime(ecCurve, public_key, trapdoor, random, (unsigned char *)argv[3], (unsigned char *)argv[4]);

            char *hexRandomPrime = BN_bn2hex(randomPrime);
            printf("%s\n", hexRandomPrime);

            free(hexRandomPrime);
            BN_free(randomPrime);
            BN_free(random);

            break;
        }
        case 2:
        {
                BIGNUM *randomPrime = BN_new();
                BN_hex2bn(&randomPrime, argv[3]);
                // create chameleon hash of the key, with r' and the subsequent key K1 || V1
                // argv[2] = K1 || V1
                // K0 = CH (K1 || V1, r')
                                        // generateChameleonHash(EC_GROUP *E, EC_POINT *Y, unsigned char *m, BIGNUM *r)
                EC_POINT *chameleonHash = generateChameleonHash(ecCurve, public_key, (unsigned char *)argv[2], randomPrime);

                char *hexRandomPrime = BN_bn2hex(randomPrime);
                char *hexChameleonHash = EC_POINT_point2hex(ecCurve, chameleonHash, POINT_CONVERSION_UNCOMPRESSED, ctx);

                // return the chameleon hash vaule
                printf("%s", hexChameleonHash);

                free(hexChameleonHash);
                free(hexRandomPrime);
                EC_POINT_free(chameleonHash);
                BN_free(randomPrime);
            break;
        }
        default:
            break;
        }
    }

    BIO_free_all(outbio);
    BN_CTX_free(ctx);
    BN_free(bnOrder);
    // BN_free(bnRandom);
    // BN_free(seed);
    BN_free(trapdoor);

    EC_GROUP_free(ecCurve);

    EC_POINT_free(public_key);
    // EC_POINT_free(chameleonHash);
    // EC_POINT_free(modifyChameleonHash);
    return 0;
}

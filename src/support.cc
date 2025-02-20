#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/x509v3.h>
#include <openssl/ecdsa.h>
 #include <openssl/bn.h>
 #include <openssl/evp.h>

#include "support.h" 
#include "certifier.pb.h" 

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <string>
using std::string;

//  Copyright (c) 2021-22, VMware Inc, and the Certifier Authors.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// -----------------------------------------------------------------------

class name_size {
public:
  const char* name_;
  int size_;
};

name_size cipher_block_byte_name_size[] = {
  {"aes-256", 16},
  {"aes-256-cbc-hmac-sha256", 16},
  {"aes-256-cbc-hmac-sha384", 16},
  {"aes-128", 16},
  {"aes-128-cbc-hmac-sha256", 16},
  {"rsa-2048-sha256-pkcs-sign", 256},
  {"rsa-2048", 256},
  {"rsa-1024-sha256-pkcs-sign", 128},
  {"rsa-1024", 128},
  {"rsa-1024-private", 128},
  {"rsa-1024-public", 128},
  {"rsa-2048-private", 256},
  {"rsa-2048-public", 256},
  {"rsa-4096-sha384-pkcs-sign", 512},
  {"rsa-4096-private", 512},
  {"rsa-4096-public", 512},
};

name_size cipher_key_byte_name_size[] = {
  {"aes-256", 32},
  {"aes-256-cbc-hmac-sha256", 64},
  {"aes-256-cbc-hmac-sha384", 64},
  {"rsa-2048-sha256-pkcs-sign", 256},
  {"rsa-2048", 256},
  {"rsa-1024-sha256-pkcs-sign", 128},
  {"rsa-1024", 128},
  {"rsa-2048-private", 256},
  {"rsa-2048-public", 256},
  {"rsa-1024-private", 128},
  {"rsa-1024-public", 128},
  {"rsa-4096-sha384-pkcs-sign", 512},
  {"rsa-4096-private", 512},
  {"rsa-4096-public", 512},
};

name_size digest_byte_name_size[] = {
  {"sha256", 32},
  {"sha-256", 32},
  {"sha-384", 48},
  {"sha-512", 64},
};

name_size mac_byte_name_size[] = {
  {"hmac-sha256", 32},
  {"aes-256-cbc-hmac-sha256", 32},
  {"aes-256-cbc-hmac-sha384", 48},
};

int cipher_block_byte_size(const char* alg_name) {
  int size = sizeof(cipher_block_byte_name_size) / sizeof(cipher_block_byte_name_size[0]);

  for (int i = 0; i < size; i++) {
    if (strcmp(alg_name, cipher_block_byte_name_size[i].name_) == 0)
      return cipher_block_byte_name_size[i].size_;
  }
  return -1;
}

int cipher_key_byte_size(const char* alg_name) {
  int size = sizeof(cipher_key_byte_name_size) / sizeof(cipher_key_byte_name_size[0]);

  for (int i = 0; i < size; i++) {
    if (strcmp(alg_name, cipher_key_byte_name_size[i].name_) == 0)
      return cipher_key_byte_name_size[i].size_;
  }
  return -1;
}

int digest_output_byte_size(const char* alg_name) {
  int size = sizeof(digest_byte_name_size) / sizeof(digest_byte_name_size[0]);

  for (int i = 0; i < size; i++) {
    if (strcmp(alg_name, digest_byte_name_size[i].name_) == 0)
      return digest_byte_name_size[i].size_;
  }
  return -1;
}

int mac_output_byte_size(const char* alg_name) {
  int size = sizeof(mac_byte_name_size) / sizeof(mac_byte_name_size[0]);

  for (int i = 0; i < size; i++) {
    if (strcmp(alg_name, mac_byte_name_size[i].name_) == 0)
      return mac_byte_name_size[i].size_;
  }
  return -1;
}

bool write_file(const string& file_name, int size, byte* data) {
  int out = open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (out < 0)
    return false;
  if (write(out, data, size) < 0) {
    printf("write failed\n");
  }
  close(out);
  return true;
}

int file_size(const string& file_name) {
  struct stat file_info;

  if (stat(file_name.c_str(), &file_info) != 0)
    return false;
  if (!S_ISREG(file_info.st_mode))
    return false;
  return (int)file_info.st_size;
}

bool read_file(const string& file_name, int* size, byte* data) {
  struct stat file_info;

  if (stat(file_name.c_str(), &file_info) != 0)
    return false;
  if (!S_ISREG(file_info.st_mode))
    return false;
  int bytes_in_file = (int)file_info.st_size;
  if (bytes_in_file > *size) {
    return false;
  }
  int fd = ::open(file_name.c_str(), O_RDONLY);
  if (fd < 0)
    return false;
  int n = (int)read(fd, data, bytes_in_file);
  close(fd);
  *size = n;
  return true;
}

// -----------------------------------------------------------------------

bool time_now(time_point* t) {
  time_t now;
  struct tm current_time;

  time(&now);
  gmtime_r(&now, &current_time);
  t->set_year(current_time.tm_year + 1900);
  t->set_month(current_time.tm_mon + 1);
  t->set_day(current_time.tm_mday);
  t->set_hour(current_time.tm_hour);
  t->set_minute(current_time.tm_min);
  t->set_seconds(current_time.tm_sec);
  return true;
}

bool time_to_string(time_point& t, string* s) {
  char t_buf[128];

  // YYYY-MM-DDTHH:mm:ss.sssZ
  int n = sprintf(t_buf, "%04d-%02d-%02dT%02d:%02d:%8.5lfZ",
      t.year(), t.month(),t.day(),
      t.hour(), t.minute(), t.seconds());
  s->assign((const char*)t_buf);
  return true;
}

bool string_to_time(const string& s, time_point* t) {
  int y, m, d, h, min;
  double secs;
  sscanf(s.c_str(), "%04d-%02d-%02dT%02d:%02d:%lfZ",
      &y, &m, &d, &h, &min, &secs);
  t->set_year(y);
  t->set_month(m);
  t->set_day(d);
  t->set_hour(h);
  t->set_minute(min);
  t->set_seconds(secs);
  return true;
}

// 1 if t1 > t2
// 0 if t1 == t2
// -1 if t1 < t2
int compare_time(time_point& t1, time_point& t2) {
  if (t1.year() > t2.year())
    return 1;
  if (t1.year() < t2.year())
    return -1;
  if (t1.month() > t2.month())
    return 1;
  if (t1.month() < t2.month())
    return -1;
  if (t1.day() > t2.day())
    return 1;
  if (t1.day() < t2.day())
    return -1;
  if (t1.hour() > t2.hour())
    return 1;
  if (t1.hour() < t2.hour())
    return -1;
  if (t1.minute() > t2.minute())
    return 1;
  if (t1.minute() < t2.minute())
    return -1;
  if (t1.seconds() > t2.seconds())
    return 1;
  if (t1.seconds() < t2.seconds())
    return -1;
  return 0;
}

bool add_interval_to_time_point(time_point& t_in, double hours, time_point* t_out) {
  int y, m, d, h, min;
  double secs;

  y = t_in.year();
  m = t_in.month();
  d = t_in.day();
  h = t_in.hour();
  min = t_in.minute();
  secs = t_in.seconds();

  int y_add = hours / (24.0 * 365.0);
  hours -= ((double)y_add) * 24.0 * 365.0;
  int m_add = hours / (24.0 * 30);
  hours -= ((double)m_add) * 24.0 * 30.0;
  int d_add = hours / 24.0;
  hours -= ((double)d_add) * 24.0;
  int h_add = hours;
  hours -= ((double)h_add);
  int min_add = hours * 60.0;
  hours -= ((double)min_add) * 60.0;
  // hours is now seconds to add

  y += y_add;
  m += m_add;
  d += d_add;
  h += h_add;
  min += min_add;
  secs += hours;
  int n;

  if (secs > 60.0) {
    n = secs / 60.0;
    min += n;
    secs -= ((double)n) * 60.0;
  }
  if (min > 60) {
    n = min / 60;
    h += n;
    h -= n;
  }
  if (h > 24) {
    n = h / 24;
    d += n;
    h -= n;
  }
  if (d > 28) {
    switch(d) {
      case 2:
        if (y%4 == 0) {
          if (d <= 29)
            break;
          d -= 29;
          m += 1;
          break;
        }
        break;
        if (d <= 28) 
          break;
        d -= 28;
        m += 1;
        break;
      case 4:
      case 6:
      case 11:
        if (d <= 30)
          break;
        d -= 30;
        m += 1;
        break;
      default:
        if (d <= 31)
          break;
        d -= 31;
        m += 1;
        break;
    }
  }
  if (m > 12) {
    m = 1;
    y += 1;
  }

  t_out->set_year(y);
  t_out->set_month(m);
  t_out->set_day(d);
  t_out->set_hour(h);
  t_out->set_minute(min);
  t_out->set_seconds(secs);
  return true;
}

void print_time_point(time_point& t) {
  printf("%02d-%02d-%02dT%02d:%02d:%8.5lfZ\n", t.year(), t.month(),
    t.day(), t.hour(), t.minute(), t.seconds());
}

// -----------------------------------------------------------------------

// Encryption is ssl
//    Set up a context
//    Initialize the encryption operation
//    Providing plaintext bytes to be encrypted
//    Finalising the encryption operation
//    During initialisation we will provide an EVP_CIPHER object.
//      In this case we are using EVP_aes_256_cbc(),
//      which uses the AES algorithm with a 256-bit key in 
//      CBC mode.

bool encrypt(byte* in, int in_len, byte *key,
            byte *iv, byte *out, int* out_size) {
  EVP_CIPHER_CTX *ctx = nullptr;
  int len = 0;
  int out_len = 0;
  bool ret = true;

  if(!(ctx = EVP_CIPHER_CTX_new())) {
      ret = false;
      goto done;
    }
  if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) {
      ret = false;
      goto done;
    }
  if(1 != EVP_EncryptUpdate(ctx, out, &len, in, in_len)) {
      ret = false;
      goto done;
    }
  out_len = len;
  if(1 != EVP_EncryptFinal_ex(ctx, out + len, &len)) {
      ret = false;
      goto done;
    }
  out_len += len;

done:
  if (ctx != nullptr)
    EVP_CIPHER_CTX_free(ctx);
  *out_size = out_len;
  return ret;
}

bool decrypt(byte *in, int in_len, byte *key,
            byte *iv, byte *out, int* size_out) {
    EVP_CIPHER_CTX *ctx = nullptr;
    int len = 0;
    int out_len = 0;
    bool ret = true;

    if(!(ctx = EVP_CIPHER_CTX_new())) {
      ret = false;
      goto done;
    }
    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) {
      ret = false;
      goto done;
    }
    if(1 != EVP_DecryptUpdate(ctx, out, &len, in, in_len)) {
      ret = false;
      goto done;
    }
    out_len = len;
    if(1 != EVP_DecryptFinal_ex(ctx, out + len, &len)) {
      ret = false;
      goto done;
    }
    out_len += len;

done:
    if (ctx != nullptr)
      EVP_CIPHER_CTX_free(ctx);
    *size_out = out_len; 
    return ret;
}

bool digest_message(const char* alg, const byte* message, int message_len,
    byte* digest, unsigned int digest_len) {

  int n = digest_output_byte_size(alg);
  if (n < 0)
    return false;
  if (n > (int)digest_len)
    return false;

  EVP_MD_CTX *mdctx;

  if (strcmp(alg, "sha-256") == 0 || strcmp(alg, "sha256") == 0) {
    if((mdctx = EVP_MD_CTX_new()) == NULL)
      return false;
    if(1 != EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL))
      return false;
  } else if (strcmp(alg, "sha-384") == 0) {
    if((mdctx = EVP_MD_CTX_new()) == NULL)
      return false;
    if(1 != EVP_DigestInit_ex(mdctx, EVP_sha384(), NULL))
      return false;
  } else if (strcmp(alg, "sha-512")  == 0) {
    if((mdctx = EVP_MD_CTX_new()) == NULL)
      return false;
    if(1 != EVP_DigestInit_ex(mdctx, EVP_sha512(), NULL))
      return false;
  } else {
    return false;
  }

  if(1 != EVP_DigestUpdate(mdctx, message, message_len))
    return false;
  if(1 != EVP_DigestFinal_ex(mdctx, digest, &digest_len))
    return false;
  EVP_MD_CTX_free(mdctx);

  return true;
}

bool authenticated_encrypt(byte* in, int in_len, byte *key,
            byte *iv, byte *out, int* out_size) {

  const char* alg_name = "aes-256-cbc-hmac-sha256";
  int blk_size =  cipher_block_byte_size(alg_name);
  int key_size =  cipher_key_byte_size(alg_name);
  int mac_size =  mac_output_byte_size(alg_name);
  int cipher_size = *out_size - blk_size;
  memset(out, 0, *out_size);

  if (!encrypt(in, in_len, key, iv, out + block_size, &cipher_size))
    return false;
  memcpy(out, iv, block_size);
  cipher_size += block_size;

  unsigned int hmac_size = mac_size;
  HMAC(EVP_sha256(), &key[key_size / 2], mac_size, out, cipher_size, out + cipher_size, &hmac_size);
  *out_size = cipher_size + hmac_size;
  return true;
}

bool authenticated_decrypt(byte* in, int in_len, byte *key,
            byte *out, int* out_size) {

  const char* alg_name = "aes-256-cbc-hmac-sha256";
  int blk_size =  cipher_block_byte_size(alg_name);
  int key_size =  cipher_key_byte_size(alg_name);
  int mac_size =  mac_output_byte_size(alg_name);
  int cipher_size = *out_size - blk_size;

  int plain_size = *out_size - blk_size - mac_size;

  int msg_with_iv_size = in_len - mac_size;
  unsigned int hmac_size = mac_size;
  byte hmac_out[hmac_size];

  HMAC(EVP_sha256(), &key[key_size / 2], mac_size, in, msg_with_iv_size, (byte*)hmac_out, &hmac_size);
  if (memcmp(hmac_out, in + msg_with_iv_size, mac_size) != 0) {
    return false;
  }

  if (!decrypt(in + block_size, msg_with_iv_size - block_size, key, in, out, &plain_size))
    return false;
  *out_size = plain_size;
  return (memcmp(hmac_out, in + msg_with_iv_size, mac_size) == 0);
}

bool authenticated_encrypt(const char* alg_name, byte* in, int in_len, byte *key,
            byte *iv, byte *out, int* out_size) {

  if (strcmp(alg_name, "aes-256-cbc-hmac-sha256") != 0 && strcmp(alg_name, "aes-256-cbc-hmac-sha384") != 0) {
    printf("Only aes-256-cbc-hmac-sha256 and aes-256-cbc-hmac-sha384 for now\n");
    return false;
  }
  int blk_size =  cipher_block_byte_size(alg_name);
  int key_size =  cipher_key_byte_size(alg_name);
  int mac_size =  mac_output_byte_size(alg_name);
  int cipher_size = *out_size - blk_size;

  memset(out, 0, *out_size);

  if (!encrypt(in, in_len, key, iv, out + block_size, &cipher_size)) {
    printf("encrypt failed\n");
    return false;
  }
  memcpy(out, iv, block_size);
  cipher_size += block_size;

  if (strcmp(alg_name, "aes-256-cbc-hmac-sha256") == 0) {
    unsigned int hmac_size = mac_size;
    HMAC(EVP_sha256(), &key[key_size / 2], mac_size, out, cipher_size, out + cipher_size, &hmac_size);
    *out_size = cipher_size + hmac_size;
  } else if (strcmp(alg_name, "aes-256-cbc-hmac-sha384") == 0) {
    unsigned int hmac_size = mac_size;
    HMAC(EVP_sha384(), &key[key_size / 2], mac_size, out, cipher_size, out + cipher_size, &hmac_size);
    *out_size = cipher_size + hmac_size;
  } else {
    return false;
  }
  return true;
}

bool authenticated_decrypt(const char* alg_name , byte* in, int in_len, byte *key,
            byte *out, int* out_size) {


  if (strcmp(alg_name, "aes-256-cbc-hmac-sha256") != 0 && strcmp(alg_name, "aes-256-cbc-hmac-sha384") != 0) {
    printf("Only aes-256-cbc-hmac-sha256 and aes-384-cbc-hmac-sha384 for now\n");
    return false;
  }

  int blk_size =  cipher_block_byte_size(alg_name);
  int key_size =  cipher_key_byte_size(alg_name);
  int mac_size =  mac_output_byte_size(alg_name);
  int cipher_size = *out_size - blk_size;

  int plain_size = *out_size - blk_size - mac_size;
  int msg_with_iv_size = in_len - mac_size;
  unsigned int hmac_size = mac_size;

  byte hmac_out[hmac_size];
  if (strcmp(alg_name, "aes-256-cbc-hmac-sha256") == 0) {
    HMAC(EVP_sha256(), &key[key_size / 2], mac_size, in, msg_with_iv_size, (byte*)hmac_out, &hmac_size);
    if (memcmp(hmac_out, in + msg_with_iv_size, mac_size) != 0) {
      return false;
    }
  } else if (strcmp(alg_name, "aes-256-cbc-hmac-sha384") == 0) {
    HMAC(EVP_sha384(), &key[key_size / 2], mac_size, in, msg_with_iv_size, (byte*)hmac_out, &hmac_size);
    if (memcmp(hmac_out, in + msg_with_iv_size, mac_size) != 0) {
      return false;
    }
  } else {
    return false;
  }

  if (!decrypt(in + block_size, msg_with_iv_size - block_size, key, in, out, &plain_size))
    return false;
  *out_size = plain_size;
  return (memcmp(hmac_out, in + msg_with_iv_size, mac_size) == 0);
}

const int rsa_alg_type = 1;
const int ecc_alg_type = 2;
bool private_key_to_public_key(const key_message& in, key_message* out) {

  int n_bytes = 0;
  int alg_type = 0;
  if (in.key_type() == "rsa-2048-private") {
    alg_type = rsa_alg_type;
    out->set_key_type("rsa-2048-public");
    n_bytes = cipher_block_byte_size("rsa-2048-public");
  } else if (in.key_type() == "rsa-1024-private") {
    alg_type = rsa_alg_type;
    out->set_key_type("rsa-1024-public");
    n_bytes = cipher_block_byte_size("rsa-1024-public");
  } else if (in.key_type() == "rsa-4096-private") {
    alg_type = rsa_alg_type;
    out->set_key_type("rsa-4096-public");
    n_bytes = cipher_block_byte_size("rsa-4096-public");
  } else if (in.key_type() == "ecc-384-private") {
    alg_type = ecc_alg_type;
    out->set_key_type("ecc-384-public");
    n_bytes = cipher_block_byte_size("ecc_384-public");
  } else {
    printf("private_key_to_public_key: bad key type\n");
    return false;
  }

  out->set_key_name(in.key_name());
  out->set_key_format(in.key_format());
  out->set_not_before(in.not_before());
  out->set_not_after(in.not_after());
  out->set_certificate(in.certificate().data(), in.certificate().size());

  if (alg_type == rsa_alg_type) {
    rsa_message* rk = new rsa_message;
    rk->set_public_modulus(in.rsa_key().public_modulus().data(),
        in.rsa_key().public_modulus().size());
    rk->set_public_exponent(in.rsa_key().public_exponent().data(),
        in.rsa_key().public_exponent().size());
    out->set_allocated_rsa_key(rk);
    return true;
  } else if (alg_type == ecc_alg_type) {
    ecc_message* ek = new ecc_message;
    ek->CopyFrom(in.ecc_key());
    ek->mutable_private_multiplier()->clear();
    out->set_allocated_ecc_key(ek);
    return true;
  } else {
    printf("private_key_to_public_key: bad key type\n");
    return false;
  }
}

bool make_certifier_rsa_key(int n,  key_message* k) {
  if (k == nullptr)
    return false;

  RSA* r = RSA_new();
  if (!generate_new_rsa_key(n, r))
    return false;

  if (n == 2048) {
    k->set_key_type("rsa-2048-private");
  } else if (n == 1024) {
    k->set_key_type("rsa-1024-private");
  } else if (n == 4096) {
    k->set_key_type("rsa-4096-private");
  } else {
    RSA_free(r);
    return false;
  }
  k->set_key_name("test-key-2");
  k->set_key_format("vse-key");
  k->set_key_type("rsa-4096-private");
  if (!RSA_to_key(r, k)) {
    return false;
  }
  RSA_free(r);
  return true;
}

bool rsa_public_encrypt(RSA* key, byte* data, int data_len, byte *encrypted, int* size_out) {
  int n = RSA_public_encrypt(data_len, data, encrypted, key, RSA_PKCS1_PADDING);
  if (n <= 0) {
    printf("rsa_public_encrypt: RSA_public_encrypt failed %d, %d\n", data_len, *size_out);
    return false;
  }
  *size_out = n; 
  return true;
}

bool rsa_private_decrypt(RSA* key, byte* enc_data, int data_len,  byte* decrypted, int* size_out) {
  int  n = RSA_private_decrypt(data_len, enc_data, decrypted, key, RSA_PKCS1_PADDING);
  if (n <= 0) {
    printf("rsa_private_decrypt: RSA_private_decrypt failed %d, %d\n", data_len, *size_out);
    return false;
  }
  *size_out = n; 
  return true;
}

//  PKCS compliant signer
bool rsa_sha256_sign(RSA* key, int to_sign_size, byte* to_sign, int* sig_size, byte* sig) {
  return rsa_sign("sha-256", key, to_sign_size, to_sign, sig_size, sig);
}

bool rsa_sha256_verify(RSA*key, int size, byte* msg, int sig_size, byte* sig) {
  return rsa_verify("sha-256", key, size, msg , sig_size, sig);
}

bool rsa_sign(const char* alg, RSA* key, int size, byte* msg, int* sig_size, byte* sig) {

  EVP_MD_CTX* sign_ctx = EVP_MD_CTX_create();
  EVP_PKEY* private_key  = EVP_PKEY_new();
  EVP_PKEY_assign_RSA(private_key, key);

  unsigned int size_digest = 0;
  if (strcmp("sha-256", alg) == 0) {
    if (EVP_DigestSignInit(sign_ctx, nullptr, EVP_sha256(), nullptr, private_key) <= 0) {
        return false;
    }
    if (EVP_DigestSignUpdate(sign_ctx, msg, size) <= 0) {
      return false;
    }
    size_t t = *sig_size;
    if (EVP_DigestSignFinal(sign_ctx, sig, &t) <= 0) {
        return false;
    }
    *sig_size = t;
  } else if(strcmp("sha-384", alg) == 0) {
    if (EVP_DigestSignInit(sign_ctx, nullptr, EVP_sha384(), nullptr, private_key) <= 0) {
        return false;
    }
    if (EVP_DigestSignUpdate(sign_ctx, msg, size) <= 0) {
      return false;
    }
    size_t t = *sig_size;
    if (EVP_DigestSignFinal(sign_ctx, sig, &t) <= 0) {
        return false;
    }
    *sig_size = t;
  } else {
    return false;
  }
  EVP_MD_CTX_destroy(sign_ctx);

  return true;
}

bool rsa_verify(const char* alg, RSA *key, int size, byte* msg, int sig_size, byte* sig) {

  if (strcmp("sha-256", alg) == 0) {
    unsigned int size_digest = digest_output_byte_size("sha-256");
    byte digest[size_digest];
    memset(digest, 0, size_digest);

    if (!digest_message("sha-256", (const byte*) msg, size, digest, size_digest))
      return false;
    int size_decrypted = RSA_size(key);
    byte decrypted[size_decrypted];
    memset(decrypted, 0, size_decrypted);
    int n = RSA_public_encrypt(sig_size, sig, decrypted, key, RSA_NO_PADDING);
    if (n < 0)
      return false;
    if (memcmp(digest, &decrypted[n - size_digest], size_digest) != 0)
      return false;

    const int check_size = 16;
    byte check_buf[16] = {
      0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    if (memcmp(check_buf, decrypted, check_size) != 0)
      return false;
    return memcmp(digest, &decrypted[n - size_digest], size_digest) == 0;
  } else if (strcmp("sha-384", alg) == 0) {
    unsigned int size_digest = digest_output_byte_size("sha-384");
    byte digest[size_digest];
    memset(digest, 0, size_digest);
    if (!digest_message("sha-384", (const byte*) msg, size, digest, size_digest)) {
      printf("digest_message failed\n");
      return false;
    }
    int size_decrypted = RSA_size(key);
    byte decrypted[size_decrypted];
    memset(decrypted, 0, size_decrypted);
    int n = RSA_public_encrypt(sig_size, sig, decrypted, key, RSA_NO_PADDING);
    if (n < 0)
      return false;
    const int check_size = 16;
    byte check_buf[16] = {
      0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    if (memcmp(check_buf, decrypted, check_size) != 0)
      return false;
    return memcmp(digest, &decrypted[n - size_digest], size_digest) == 0;
  } else {
    return false;
  }
}

bool generate_new_rsa_key(int num_bits, RSA* r) {
  bool ret= true;
  BIGNUM* bne = NULL;
  uint32_t e = RSA_F4;

  bne = BN_new();
  if (1 != BN_set_word(bne, e)) {
    ret = false;
    goto done;
  }
  if (1 != RSA_generate_key_ex(r, num_bits, bne, NULL)) {
    ret = false;
    goto done;
  }

done:
  BN_free(bne);
  return ret;
}

bool key_to_RSA(const key_message& k, RSA* r) {
  if (k.key_format() != "vse-key") {
    return false;
  }

  bool private_key= true;
  int key_size_bits= 0;
  if (k.key_type() == "rsa-1024-public") {
    key_size_bits= 1024;
    private_key = false;
  } else if (k.key_type() == "rsa-1024-private") {
    key_size_bits= 1024;
    private_key = true;
  } else if (k.key_type() == "rsa-2048-public") {
    key_size_bits= 2048;
    private_key = false;
  } else if (k.key_type() == "rsa-2048-private") {
    key_size_bits= 2048;
    private_key = true;
  } else if (k.key_type() == "rsa-4096-private") {
    key_size_bits= 4096;
    private_key = true;
  } else if (k.key_type() == "rsa-4096-public") {
    key_size_bits= 4096;
    private_key = false;
  } else {
    return false;
  }

  if (!k.has_rsa_key()) {
    return false;
  }
  const rsa_message& rsa_key_data = k.rsa_key();
  if (!rsa_key_data.has_public_modulus() || !rsa_key_data.has_public_exponent()) {
    print_key(k);
    return false;
  }
  BIGNUM* n =  BN_bin2bn((byte*)(rsa_key_data.public_modulus().data()),
                (int)(rsa_key_data.public_modulus().size()), NULL);
  BIGNUM* e =  BN_bin2bn((const byte*) rsa_key_data.public_exponent().data(),
                (int)rsa_key_data.public_exponent().size(), NULL);
  BIGNUM* d = nullptr;
  if (private_key && rsa_key_data.has_private_exponent()) {
    d =  BN_bin2bn((const byte*) rsa_key_data.private_exponent().data(),
                (int)rsa_key_data.private_exponent().size(), NULL);
  }
  if (1 != RSA_set0_key(r, n, e, d)) {
    return false;
  }
  return true;
}

bool RSA_to_key(RSA* r, key_message* k) {
  const BIGNUM* m = nullptr;
  const BIGNUM* e = nullptr;
  const BIGNUM* d = nullptr;
  
  k->set_key_format("vse-key");
  RSA_get0_key(r, &m, &e, &d);

  int rsa_size = RSA_bits(r);
  if (rsa_size == 1024) {
    if (d == nullptr)
      k->set_key_type("rsa-1024-public");
    else
      k->set_key_type("rsa-1024-private");
  } else if (rsa_size == 2048) {
    if (d == nullptr)
      k->set_key_type("rsa-2048-public");
    else
      k->set_key_type("rsa-2048-private");
  } else if (rsa_size == 4096) {
    if (d == nullptr)
      k->set_key_type("rsa-4096-public");
    else
      k->set_key_type("rsa-4096-private");
  } else {
    return false;
  }
  rsa_message* rsa= new(rsa_message);
  k->set_allocated_rsa_key(rsa);

  int i;
  int size;
  if (m != nullptr) {
    size = BN_num_bytes(m);
    byte n_b[size];
    memset(n_b, 0, size);
    i = BN_bn2bin(m, n_b);
    rsa->set_public_modulus((void*)n_b, i);
  }
  if (e != nullptr) {
    size = BN_num_bytes(e);
    byte e_b[size];
    memset(e_b, 0, size);
    i = BN_bn2bin(e, e_b);
    rsa->set_public_exponent((void*)e_b, i);
  }
  if (d != nullptr) {
    size = BN_num_bytes(d);
    byte d_b[size];
    memset(d_b, 0, size);
    i = BN_bn2bin(d, d_b);
    rsa->set_private_exponent((void*)d_b, i);
  }
  return true;
}

void print_point(const point_message& pt) {
  if (!pt.has_x() || !pt.has_y())
    return;

  BIGNUM* x = BN_new();
  BIGNUM* y = BN_new();

  BN_bin2bn((byte*)pt.x().data(), pt.x().size(), x);
  BN_bin2bn((byte*)pt.y().data(), pt.y().size(), y);
  printf("(");
  BN_print_fp(stdout, x);
  printf(", ");
  BN_print_fp(stdout, y);
  printf(")");

  BN_free(x);
  BN_free(y);
}

void print_ecc_key(const ecc_message& em) {

  if (em.has_curve_name()) {
    printf("Curve name: %s\n", em.curve_name().c_str());
  }

  if (em.has_public_point()) {
    printf("Public point:\n");
    print_point(em.public_point());
    printf("\n");
  }

  if (em.has_base_point()) {
    printf("Base   point:\n");
    print_point(em.base_point());
    printf("\n");
  }

  if (em.has_order_of_base_point()) {
    BIGNUM* order = BN_new();
    BN_bin2bn((byte*)em.order_of_base_point().data(), em.order_of_base_point().size(), order);
    printf("order: ");
    BN_print_fp(stdout, order);
    printf("\n");
    BN_free(order);
  }

  if (em.has_private_multiplier()) {
    BIGNUM* private_mult = BN_new();

    BN_bin2bn((byte*)em.private_multiplier().data(), em.private_multiplier().size(), private_mult);
    printf("private multiplier: ");
    BN_print_fp(stdout, private_mult);
    printf("\n");

    BN_free(private_mult);
  } else {
    printf("No private multiplier\n");
  }

  if (em.has_curve_p() && em.has_curve_p() && em.has_curve_p()) {
    BIGNUM* p = BN_new();
    BIGNUM* a = BN_new();
    BIGNUM* b = BN_new();
  
    BN_bin2bn((byte*)em.curve_p().data(), em.curve_p().size(), p);
    BN_bin2bn((byte*)em.curve_a().data(), em.curve_a().size(), a);
    BN_bin2bn((byte*)em.curve_b().data(), em.curve_b().size(), b);

    printf("Curve parameters:\n");
    printf("  p: ");
    BN_print_fp(stdout, p);
    printf("\n");
    printf("  a: ");
    BN_print_fp(stdout, a);
    printf("\n");
    printf("  b: ");
    BN_print_fp(stdout, b);
    printf("\n");

    BN_free(p);
    BN_free(a);
    BN_free(b);
  }
}

//  ECC encrypt
//    G is generator, x is private key P=xG ia public key
//    Encrypt
//      Embed message m in P_m.  Pick random k.  Send (kG, kP + P_m)
//    Decrypt
//      compute Q=xkG = kP.  Subtract Q from kP + P_m = P_m.  Extract message from P_m.

bool ecc_sign(const char* alg, EC_KEY* key, int size, byte* msg, int* size_out, byte* out) {
  unsigned int len = (unsigned int)digest_output_byte_size(alg);
  byte digest[len];

  int blk_len = ECDSA_size(key);
  if (*size_out < 2 * blk_len)
    return false;

  if (!digest_message(alg, msg, size, digest, len)) {
    return false;
  }
  unsigned int sz = (unsigned int) *size_out;
  if (ECDSA_sign(0, digest, len, out, &sz, key) != 1) {
    return false;
  }
  *size_out = (int) sz;
  return true;
}

bool ecc_verify(const char* alg, EC_KEY* key, int size, byte* msg, int size_sig, byte* sig) {
  unsigned int len = (unsigned int)digest_output_byte_size(alg);
  byte digest[len];

  if (!digest_message(alg, msg, size, digest, len)) {
    return false;
  }
  int res = ECDSA_verify(0, digest, len, sig, size_sig, key);
  if (res != 1) {
    return false;
  }
  return true;
}

EC_KEY* generate_new_ecc_key(int num_bits) {

  if (num_bits != 384) {
    printf("Only P-384 supported\n");
    return nullptr;
  }
  EC_KEY* ecc_key = EC_KEY_new_by_curve_name(NID_secp384r1);
  if (ecc_key == nullptr) {
    printf("Can't get curve by name\n");
    return nullptr;
  }

  if (1 != EC_KEY_generate_key(ecc_key)) {
    printf("Can't generate key\n");
    return nullptr;
  }

#if 0
  BN_CTX* ctx = BN_CTX_new();
  const EC_GROUP* group = EC_KEY_get0_group(ecc_key);
  if (group == nullptr) {
    printf("Can't get group (1)\n");
    return nullptr;
  }
  BIGNUM* pt_x = BN_new();
  BIGNUM* pt_y = BN_new();
  const EC_POINT* pt = EC_KEY_get0_public_key(ecc_key);
  EC_POINT_get_affine_coordinates_GFp(group, pt, pt_x, pt_y, ctx); 
  BN_CTX_free(ctx);
#endif
  return ecc_key;
}

// Todo: free k on error
EC_KEY* key_to_ECC(const key_message& k) {
  if (k.key_type() != "ecc-384-private" && k.key_type() != "ecc-384-public") {
    printf("key_to_ECC: wrong type %s\n", k.key_type().c_str());
    return nullptr;
  }
  EC_KEY* ecc_key = EC_KEY_new_by_curve_name(NID_secp384r1);
  if (ecc_key == nullptr) {
    printf("Can't get curve by name\n");
    return nullptr;
  }

  // set private multiplier
  const BIGNUM* priv_mult =  BN_bin2bn((byte*)(k.ecc_key().private_multiplier().data()),
                (int)(k.ecc_key().private_multiplier().size()), NULL);
  if (priv_mult == nullptr) {
    printf("key_to_ECC: not private mult\n");
    return nullptr;
  }
  if (EC_KEY_set_private_key(ecc_key, priv_mult) != 1) {
    printf("key_to_ECC: not can't set\n");
    return nullptr;
  }

  // set public point
  const EC_GROUP* group = EC_KEY_get0_group(ecc_key);
  if (group == nullptr) {
    printf("Can't get group (1)\n");
    return nullptr;
  }
  const BIGNUM* p_pt_x =  BN_bin2bn((byte*)(k.ecc_key().public_point().x().data()),
                (int)(k.ecc_key().public_point().x().size()), NULL);
  const BIGNUM* p_pt_y =  BN_bin2bn((byte*)(k.ecc_key().public_point().y().data()),
                (int)(k.ecc_key().public_point().y().size()), NULL);
  if (p_pt_x == nullptr || p_pt_y == nullptr) {
    printf("key_to_ECC: pts are null\n");
    return nullptr;
  }

  EC_POINT* pt = EC_POINT_new(group);
  if (pt == nullptr) {
    printf("key_to_ECC: no pt in group\n");
    return nullptr;
  }
  BN_CTX* ctx = BN_CTX_new();
  if (ctx == nullptr) {
    return nullptr;
  }
  if (EC_POINT_set_affine_coordinates_GFp(group, pt, p_pt_x, p_pt_y, ctx) != 1) {
    printf("key_to_ECC: can't set affine\n");
    return nullptr;
  }
  if (EC_KEY_set_public_key(ecc_key, pt) != 1) {
    printf("key_to_ECC: can't set public\n");
    return nullptr;
  }
  BN_CTX_free(ctx);

  return ecc_key;
}

bool ECC_to_key(const EC_KEY* ecc_key, key_message* k) {

  k->set_key_name("ecc-384-private");
  k->set_key_format("vse_key");

  ecc_message* ek = new ecc_message;
  if (ek == nullptr)
    return false;

  if (ecc_key == nullptr) {
    return false;
  }

  BN_CTX* ctx = BN_CTX_new();
  if (ctx == nullptr)
    return false;

  const EC_GROUP* group = EC_KEY_get0_group(ecc_key);
  if (group == nullptr) {
    printf("Can't get group (2)\n");
    return false;
  }

  BIGNUM* p = BN_new();
  BIGNUM* a = BN_new();
  BIGNUM* b = BN_new();
  if (EC_GROUP_get_curve(group, p, a, b, ctx) <= 0) {
    BN_CTX_free(ctx);
    return false;
  }

  if (BN_num_bytes(p) == 48) {
    ek->set_curve_name("P-384");
  } else {
    printf("Only P-384 supported\n");
    return false;
  }

  // set p, a, b
  int sz  = BN_num_bytes(p);
  byte p_buf[sz];
  sz  = BN_bn2bin(p, p_buf);
  ek->mutable_curve_p()->assign((char*)p_buf, sz);

  sz  = BN_num_bytes(a);
  byte a_buf[sz];
  sz  = BN_bn2bin(a, a_buf);
  ek->mutable_curve_a()->assign((char*)a_buf, sz);

  sz  = BN_num_bytes(b);
  byte b_buf[sz];
  sz  = BN_bn2bin(b, b_buf);
  ek->mutable_curve_b()->assign((char*)b_buf, sz);

  BN_free(p);
  BN_free(a);
  BN_free(b);

  // set base_point
  const EC_POINT* generator = EC_GROUP_get0_generator(group);
  if (generator == nullptr) {
    printf("Can't get base point\n");
    BN_CTX_free(ctx);
    return false;
  }
  BIGNUM* x = BN_new();
  BIGNUM* y = BN_new();
  if (EC_POINT_get_affine_coordinates_GFp(group,
        generator, x, y, ctx) != 1) {
    printf("Can't get affine coordinates\n");
    BN_CTX_free(ctx);
    return false;
  }

  sz  = BN_num_bytes(x);
  byte x_buf[sz];
  sz  = BN_bn2bin(x, x_buf);
  point_message* b_pt= new point_message;
  ek->set_allocated_base_point(b_pt);
  b_pt->set_x((void*)x_buf, sz);

  sz  = BN_num_bytes(y);
  byte y_buf[sz];
  sz  = BN_bn2bin(y, y_buf);
  b_pt->set_y((void*)y_buf, sz);
  BN_free(x);
  BN_free(y);

  // set public_point
  const EC_POINT* pub_pt= EC_KEY_get0_public_key(ecc_key);
  if (pub_pt == nullptr) {
    printf("Can't get public point\n");
    BN_CTX_free(ctx);
    return false;
  }

  BIGNUM* xx = BN_new();
  BIGNUM* yy = BN_new();
  if (EC_POINT_get_affine_coordinates_GFp(group,
        pub_pt, xx, yy, ctx) != 1) {
    printf("Can't get affine coordinates\n");
    BN_CTX_free(ctx);
    return false;
  }
  sz  = BN_num_bytes(xx);
  byte xx_buf[sz];
  sz  = BN_bn2bin(xx, xx_buf);
  point_message* p_pt= new point_message;
  ek->set_allocated_public_point(p_pt);
  p_pt->set_x((void*)xx_buf, sz);
  sz  = BN_num_bytes(yy);
  byte yy_buf[sz];
  sz  = BN_bn2bin(yy, yy_buf);
  p_pt->set_y((void*)yy_buf, sz);
  BN_free(xx);
  BN_free(yy);

  // set order_of_base_point
  BIGNUM* order = BN_new();
  if (EC_GROUP_get_order(group, order, ctx) != 1) {
    printf("Can't get order\n");
    BN_free(order);
    BN_CTX_free(ctx);
    return false;
  }
  sz  = BN_num_bytes(order);
  byte order_buf[sz];
  sz  = BN_bn2bin(order, order_buf);
  ek->set_order_of_base_point((void*)order_buf, sz);
  BN_free(order);

  // set private_multiplier
  const BIGNUM* pk = EC_KEY_get0_private_key(ecc_key);
  if (pk != nullptr) {
    sz  = BN_num_bytes(pk);
    byte pm_buf[sz];
    sz  = BN_bn2bin(pk, pm_buf);
    ek->set_private_multiplier((void*)pm_buf, sz);
  }

  k->set_allocated_ecc_key(ek);
  BN_CTX_free(ctx);
  return true;
}

bool make_certifier_ecc_key(int n,  key_message* k) {
  if (k == nullptr)
    return false;
  if (n != 384)
    return false;

  EC_KEY* ek = generate_new_ecc_key(n);
  if (ek == nullptr)
    return false;

  k->set_key_name("test-key-2");
  k->set_key_format("vse-key");
  k->set_key_type("ecc-384-private");
  if (!ECC_to_key(ek, k)) {
    return false;
  }
  EC_KEY_free(ek);
  return true;
}

// -----------------------------------------------------------------------

bool get_random(int num_bits, byte* out) {
  bool ret = true;

#if 1
  int in = open("/dev/urandom", O_RDONLY, 0644);
#else
  // FIX! it should be /dev/random
  int in = open("/dev/random", O_RDONLY, 0644);
#endif
  int n = ((num_bits + num_bits_in_byte - 1) / num_bits_in_byte);
  if (read(in, out, n) != n)
    ret = false;
  close(in);
  return ret;
}

// may want to check leading 0's
bool same_point(const point_message& pt1, const point_message& pt2) {
  if (pt1.x().size() != pt2.x().size())
    return false;
  if (pt1.y().size() != pt2.y().size())
    return false;
  if (memcmp(pt1.x().data(),pt1.x().data(), pt1.x().size()) != 0)
    return false;
  if (memcmp(pt1.y().data(),pt1.y().data(), pt1.y().size()) != 0)
    return false;
  return true;
}

bool same_key(const key_message& k1, const key_message& k2) {
  if (k1.key_type() != k2.key_type())
    return false;
  if (k1.key_type() == "rsa-2048-private" || k1.key_type() == "rsa-2048-public" ||
      k1.key_type() == "rsa-1024-private" || k1.key_type() == "rsa-1024-public" ||
      k1.key_type() == "rsa-4096-private" || k1.key_type() == "rsa-4096-public") {
    string b1, b2;
    if (!k1.has_rsa_key() || !k2.has_rsa_key())
      return false;
    if (k1.rsa_key().public_modulus() != k2.rsa_key().public_modulus())
      return false;
    if (k1.rsa_key().public_exponent() != k2.rsa_key().public_exponent())
      return false;
    return true;
  } else if (k1.key_type() == "aes-256-cbc-hmac-sha256" || k1.key_type() == "aes-256-cbc" || k1.key_type() == "aes-256") {
    if (!k1.has_secret_key_bits())
      return false;
    if (k1.secret_key_bits().size() != k2.secret_key_bits().size())
      return false;
    return (memcmp(k1.secret_key_bits().data(), k2.secret_key_bits().data(), k1.secret_key_bits().size()) == 0);
  } else if (k1.key_type() == "ecc-384") {
    const ecc_message& em1 = k1.ecc_key();
    const ecc_message& em2 = k2.ecc_key();
    if (em1.curve_p().size() != em2.curve_p().size() ||
        memcmp(em1.curve_p().data(),em2.curve_p().data(), em1.curve_p().size()) != 0)
      return false;
    if (em1.curve_a().size() != em2.curve_a().size() ||
          memcmp(em1.curve_a().data(),em1.curve_a().data(), em2.curve_a().size()) != 0)
      return false;
    if (em1.curve_b().size() != em2.curve_b().size() ||
           memcmp(em1.curve_b().data(),em1.curve_b().data(), em2.curve_b().size()) != 0)
      return false;
    if (!same_point(em1.base_point(), em2.base_point()))
      return false;
    if (!same_point(em1.public_point(), em2.public_point()))
      return false;
    return true;
  } else {
    return false;
  }
  return true;
}

bool same_measurement(string& m1, string& m2) {
  if (m1.size() != m2.size())
    return false;
  if (memcmp((byte*)m1.data(), (byte*)m2.data(), m1.size()) != 0)
    return false;
  return true;
}

bool same_entity(const entity_message& e1, const entity_message& e2) {
  if (e1.entity_type() != e2.entity_type())
    return false;
  if (e1.entity_type() == "key")
    return same_key(e1.key(), e2.key());
  if (e1.entity_type() == "measurement") {
    string s1;
    string s2;
    s1.assign((char*)e1.measurement().data(), e1.measurement().size());
    s2.assign((char*)e2.measurement().data(), e2.measurement().size());
    return same_measurement(s1, s2);
  }
  return false;
}

bool same_vse_claim(const vse_clause& c1, const vse_clause& c2) {
  if (c1.has_subject() != c2.has_subject() || c1.has_object() != c2.has_object() ||
      c1.has_verb() != c2.has_verb() || c1.has_clause() != c2.has_clause())
    return false;

  if (c1.has_subject()) {
      if (!same_entity(c1.subject(), c2.subject()))
        return false;
  }

  if (c1.has_verb()) {
      if (c1.verb() != c2.verb())
        return false;
  }

  if (c1.has_object()) {
      if (!same_entity(c1.object(), c2.object()))
        return false;
  }

  if (c1.has_clause())
    return same_vse_claim(c1.clause(), c2.clause());

  return true;
}

bool make_key_entity(const key_message& key, entity_message* ent) {
  ent->set_entity_type("key");
  key_message* k = new(key_message);
  k->CopyFrom(key);
  ent->set_allocated_key(k);
  return true;
}

bool make_measurement_entity(string& measurement, entity_message* ent) {
  ent->set_entity_type("measurement");
  string* m = new string(measurement);
  ent->set_allocated_measurement(m);
  return true;
}

bool make_unary_vse_clause(const entity_message& subject, string& verb,
    vse_clause* out) {
  entity_message* s= new(entity_message);
  s->CopyFrom(subject);
  out->set_allocated_subject(s);
  out->set_verb(verb);
  return true;
}

bool make_simple_vse_clause(const entity_message& subject, string& verb,
    const entity_message& object, vse_clause* out) {
  entity_message* s= new(entity_message);
  s->CopyFrom(subject);
  out->set_allocated_subject(s);
  entity_message* o= new(entity_message);
  o->CopyFrom(object);
  out->set_allocated_object(o);
  out->set_verb(verb);
  return true;
}

bool make_indirect_vse_clause(const entity_message& subject, string& verb,
    const vse_clause& in, vse_clause* out) {
  entity_message* s = new(entity_message);
  s->CopyFrom(subject);
  out->set_allocated_subject(s);
  vse_clause* cl = new(vse_clause);
  cl->CopyFrom(in);
  out->set_allocated_clause(cl);
  out->set_verb(verb);
  return true;
}

bool make_claim(int size, byte* serialized_claim, string& format, string& descriptor,
    string& not_before, string& not_after, claim_message* out) {
  out->set_claim_format(format);
  out->set_claim_descriptor(descriptor);
  out->set_not_before(not_before);
  out->set_not_after(not_after);
  out->set_serialized_claim((void*)serialized_claim, size);
  return true;
}

// -----------------------------------------------------------------------

void print_bytes(int n, byte* buf) {
  for(int i = 0; i < n; i++)
    printf("%02x", buf[i]);
}

void print_rsa_key(const rsa_message& rsa) {
  if (rsa.has_public_modulus()) {
    printf("Modulus: ");
    print_bytes(rsa.public_modulus().size(), (byte*)rsa.public_modulus().data());
    printf("\n");
  }
  if (rsa.has_public_exponent()) {
    printf("Public exponent: ");
    print_bytes(rsa.public_exponent().size(), (byte*)rsa.public_exponent().data());
    printf("\n");
  }
  if (rsa.has_private_exponent()) {
    printf("Private exponent: ");
    print_bytes(rsa.private_exponent().size(), (byte*)rsa.private_exponent().data());
    printf("\n");
  }
  if (rsa.has_private_p()) {
  }
  if (rsa.has_private_q()) {
  }
  if (rsa.has_private_dp()) {
  }
  if (rsa.has_private_dq()) {
  }
}

void print_key(const key_message& k) {
  if (k.has_key_name()) {
    printf("Key name: %s\n", k.key_name().c_str());
  }
  if (k.has_key_type()) {
    printf("Key type: %s\n", k.key_type().c_str());
  }
  if (k.has_key_format()) {
    printf("Key format: %s\n", k.key_format().c_str());
  }
  if (k.has_rsa_key()) {
    print_rsa_key(k.rsa_key());
  }
  if (k.has_ecc_key()) {
    print_ecc_key(k.ecc_key());
  }
  if (k.has_secret_key_bits()) {
    printf("Secret key bits: ");
    print_bytes(k.secret_key_bits().size(), (byte*)k.secret_key_bits().data());
    printf("\n");
  }
  if (k.has_certificate() && k.certificate().size() > 0) {
    X509* cert= X509_new();
    if (cert == nullptr)
      return;
    string in;
    in.assign((char*)k.certificate().data(), k.certificate().size());
    if (!asn1_to_x509(in, cert)) {
      X509_free(cert);
      return;
    }
    X509_print_fp(stdout, cert);
    X509_free(cert);
  }
}

void print_key_descriptor(const key_message& k) {

  if (!k.has_key_type())
    return;

  if (k.key_type() == "rsa-2048-private" || k.key_type() == "rsa-2048-public" ||
      k.key_type() == "rsa-1024-private" || k.key_type() == "rsa-1024-public" ||
      k.key_type() == "rsa-4096-private" || k.key_type() == "rsa-4096-public") {
    printf("Key[rsa, ");
    if (k.has_key_name()) {
      printf("%s, ", k.key_name().c_str());
    }
    if (k.has_rsa_key()) {
      int l = (int)k.rsa_key().public_modulus().size();
      if (l > 20)
        l = 20;
      if (k.rsa_key().has_public_modulus()) {
        print_bytes(l, (byte*)k.rsa_key().public_modulus().data());
      }
      printf("]");
    }
  } else if (k.key_type() == "ecc-384-private" || k.key_type() == "ecc-384-public") {
    printf("Key[ecc, ");
    if (k.has_key_name()) {
      printf("%s, ", k.key_name().c_str());
    }
    if (k.has_ecc_key()) {
      printf("%s-", k.ecc_key().curve_name().c_str());
      print_bytes(k.ecc_key().base_point().x().size(), (byte*)k.ecc_key().base_point().x().data());
      printf("_");
      print_bytes(k.ecc_key().base_point().y().size(), (byte*)k.ecc_key().base_point().y().data());
      printf("]");
    }
  } else {
    printf(" unsupported type %s ", k.key_type().c_str());
  }
}

void print_entity_descriptor(const entity_message& e) {
  if (e.entity_type() == "key" && e.has_key()) {
    print_key_descriptor(e.key());
  } else if (e.entity_type() == "measurement" && e.has_measurement()) {
    printf("Measurement[");
    print_bytes((int)e.measurement().size(), (byte*)e.measurement().data());
    printf("] ");
  } else {
  }
}

void print_vse_clause(const vse_clause c) {
  if (c.has_subject()) {
    print_entity_descriptor(c.subject());
    printf(" ");
  }
  if (c.has_verb()) {
    printf("%s ", c.verb().c_str());
  }
  if (c.has_object()) {
    print_entity_descriptor(c.object());
    printf(" ");
  }
  if (c.has_clause()) {
    print_vse_clause(c.clause());
    printf(" ");
  }
}

void print_claim(const claim_message& claim) {
  if (!claim.has_claim_format()) {
    return;
  }
  printf("format: %s\n", claim.claim_format().c_str());
  if (claim.has_claim_descriptor()) {
    printf("%s\n", claim.claim_descriptor().c_str());
  }
  if (claim.has_not_before()) {
    printf("not before: %s\n", claim.not_before().c_str());
  }
  if (claim.has_not_after()) {
    printf("not after: %s\n", claim.not_after().c_str());
  }
  if (claim.claim_format() == "vse-clause" && claim.has_serialized_claim()) {
    vse_clause c;
    c.ParseFromString(claim.serialized_claim());
    print_vse_clause(c);
    printf("\n");
  }
  if (claim.claim_format() == "vse-attestation" && claim.has_serialized_claim()) {
    attestation at;
    at.ParseFromString(claim.serialized_claim());
    print_attestation(at);
    printf("\n");
  }
}

void print_attestation(attestation& at) {
  if (at.has_description())
    printf("Description: %s\n", at.description().c_str());
  if (at.has_key_id())
    printf("Key-id: %s\n", at.key_id().c_str());
  if (at.has_measurement()) {
    printf("Measurement: ");
    print_bytes((int)at.measurement().size(), (byte*)at.measurement().data());
    printf("\n");
  }
  if (at.has_time())
    printf("time attested: %s\n", at.time().c_str());
  if (at.has_clause()) {
    print_vse_clause(at.clause());
  }
}

void print_signed_claim(const signed_claim_message& signed_claim) {
  printf("\nSigned claim\n");
  if (!signed_claim.has_serialized_claim_message())
    return;
  claim_message cl;
  string s_claim;
  s_claim.assign((char*)signed_claim.serialized_claim_message().data(), 
      signed_claim.serialized_claim_message().size());
  cl.ParseFromString(s_claim);
  print_claim(cl);
  printf("Serialized: "); print_bytes((int)signed_claim.serialized_claim_message().size(),
    (byte*)signed_claim.serialized_claim_message().data()); printf("\n");
  if (signed_claim.has_signing_key()) {
    printf("Signer key: "); print_key(signed_claim.signing_key()); printf("\n");
  }
  if (signed_claim.has_signing_algorithm()) {
    printf("Signing algorithm: %s\n", signed_claim.signing_algorithm().c_str());
  }
  if (signed_claim.has_signature()) {
    printf("Signature: ");
    print_bytes((int)signed_claim.signature().size(),
      (byte*)signed_claim.signature().data());
    printf("\n");
  }

  if (cl.claim_format() == "vse-clause") {
    vse_clause v1;
    string serialized_vse;
    serialized_vse.assign((char*)cl.serialized_claim().data(),
        cl.serialized_claim().size());
    if (v1.ParseFromString(serialized_vse)) {
      print_vse_clause(v1);
      printf("\n");
    }
  }
}

void print_entity(const entity_message& em) {
  if (!em.has_entity_type())
    printf("%s entity\n", em.entity_type().c_str());
  if (em.entity_type() == "key") {
    print_key(em.key());
  } else if (em.entity_type() == "measurement") {
    printf("Measurement[");
    print_bytes((int)em.measurement().size(), (byte*)em.measurement().data());
    printf("] ");
  } else {
    return;
  }
}

bool verify_signed_attestation(int serialized_size, byte* serialized,
      int sig_size, byte* sig, const key_message& key) {
  attestation at;
  string s;
  s.assign((char*)serialized, serialized_size);
  at.ParseFromString(s);

  RSA* r = RSA_new();
  if (!key_to_RSA(key, r))
    return false;
  bool fRet= rsa_sha256_verify(r, serialized_size, serialized, sig_size, sig);
  RSA_free(r);
  return fRet;
}

bool make_signed_claim( const char* alg, const claim_message& claim, const key_message& key,
    signed_claim_message* out) {

  string serialized_claim;
  if(!claim.SerializeToString(&serialized_claim)) {
    printf("make_signed_claim: serialize claim failed\n");
    return false;
  }

  out->set_signing_algorithm(alg);
  out->set_serialized_claim_message((void*)serialized_claim.data(), serialized_claim.size());

  int sig_size = 0;
  bool success = false;
  if (strcmp(alg, "rsa-2048-sha256-pkcs-sign") == 0) {
    RSA* r = RSA_new();
    if (!key_to_RSA(key, r)) {
      printf("make_signed_claim: key_to_RSA failed\n");
      return false;
    }

    sig_size = RSA_size(r);
    byte sig[sig_size];
    success = rsa_sha256_sign(r, serialized_claim.size(), (byte*)serialized_claim.data(),
      &sig_size, sig);
    RSA_free(r);

    // sign serialized claim
    key_message* psk = new key_message;
    if (!private_key_to_public_key(key, psk)) {
      printf("make_signed_claim: private_key_to_public_key failed\n");
      return false;
    }
    out->set_allocated_signing_key(psk);
    out->set_signing_algorithm(alg);
    out->set_signature((void*)sig, sig_size);
  } else if (strcmp(alg, "rsa-4096-sha384-pkcs-sign") == 0) {
    RSA* r = RSA_new();
    if (!key_to_RSA(key, r)) {
      printf("make_signed_claim: key_to_RSA failed\n");
      return false;
    }

    sig_size = RSA_size(r);
    byte sig[sig_size];
    success = rsa_sign("sha-384", r, serialized_claim.size(), (byte*)serialized_claim.data(),
      &sig_size, sig);
    if (!success) {
      printf("make_signed_claim: rsa_sign failed\n");
    }
    RSA_free(r);

    // sign serialized claim
    key_message* psk = new key_message;
    if (!private_key_to_public_key(key, psk)) {
      printf("make_signed_claim: private_key_to_public_key failed\n");
      return false;
    }
    out->set_allocated_signing_key(psk);
    out->set_signing_algorithm(alg);
    out->set_signature((void*)sig, sig_size);
  } else if (strcmp(alg, "ecc-384-sha384-pkcs-sign") == 0) {
    EC_KEY* k = key_to_ECC(key);
    if (k == nullptr) {
      printf("make_signed_claim: to_ECC failed\n");
      return false;
    }
    sig_size = 2 * ECDSA_size(k);
    byte sig[sig_size];

    success = ecc_sign("sha-384", k, serialized_claim.size(), (byte*)serialized_claim.data(),
      &sig_size, sig);
    EC_KEY_free(k);

    // sign serialized claim
    key_message* psk = new key_message;
    if (!private_key_to_public_key(key, psk))
      return false;
    out->set_allocated_signing_key(psk);
    out->set_signature((void*)sig, sig_size);
  } else {
    return false;
  }
  return success;
}

bool verify_signed_claim(const signed_claim_message& signed_claim, const key_message& key) {
  if (!signed_claim.has_serialized_claim_message() || !signed_claim.has_signing_key() ||
      !signed_claim.has_signing_algorithm() || !signed_claim.has_signature())
    return false;

  string serialized_claim;
  serialized_claim.assign((char*)signed_claim.serialized_claim_message().data(),
    signed_claim.serialized_claim_message().size());
  claim_message c;
  if (!c.ParseFromString(serialized_claim))
    return false;

  if (!c.has_claim_format())
    return false;
  if (c.claim_format() != "vse-clause" && c.claim_format() != "vse-attestation") {
    printf("%s should be vse-clause or vse-attestation\n", c.claim_format().c_str());        
    return false;
  }

  time_point t_now; 
  time_point t_nb;
  time_point t_na;

  if (!time_now(&t_now))
    return false;
  if (!string_to_time(c.not_before(), &t_nb))
    return false;
  if (!string_to_time(c.not_after(), &t_na))
    return false;

  if (compare_time(t_now, t_nb) <  0)
     return false;
  if (compare_time(t_na, t_now) < 0)
     return false;

  bool success = false;
  if (signed_claim.signing_algorithm() == "rsa-2048-sha256-pkcs-sign") {
    RSA* r = RSA_new();
    if (!key_to_RSA(key, r))
      return false;
    success = rsa_sha256_verify(r, (int)signed_claim.serialized_claim_message().size(),
        (byte*)signed_claim.serialized_claim_message().data(), (int)signed_claim.signature().size(),
        (byte*)signed_claim.signature().data());
    RSA_free(r);
  } else if (signed_claim.signing_algorithm() == "rsa-4096-sha384-pkcs-sign") {
    RSA* r = RSA_new();
    if (!key_to_RSA(key, r)) {
      return false;
    }
    success = rsa_verify("sha-384", r, (int)signed_claim.serialized_claim_message().size(),
        (byte*)signed_claim.serialized_claim_message().data(), (int)signed_claim.signature().size(),
        (byte*)signed_claim.signature().data());
    RSA_free(r);
    return success;
  } else if (signed_claim.signing_algorithm() == "ecc-384-sha384-pkcs-sign") {
    EC_KEY* k = key_to_ECC(key);
    if (k == nullptr)
      return false;
    success = ecc_verify("sha-384", k, (int)signed_claim.serialized_claim_message().size(),
        (byte*)signed_claim.serialized_claim_message().data(), (int)signed_claim.signature().size(),
        (byte*)signed_claim.signature().data());
    EC_KEY_free(k);
  } else {
    return false;
  }

  return success;
}

// -----------------------------------------------------------------------

bool vse_attestation(const string& descript, const string& enclave_type,
         const string& enclave_id, vse_clause& cl, string* serialized_attestation) {
  attestation at;

  at.set_description(descript);
  at.set_enclave_type(enclave_type);
  int digest_size = digest_output_byte_size("sha-256");
  int size_out= digest_size;
  byte m[size_out];
  memset(m, 0, size_out);
  if (!Getmeasurement(enclave_type, enclave_id, &size_out, m))
    return false;
  at.set_measurement((void*)m, size_out);
  time_point t_now;
  if (!time_now(&t_now))
    return false;
  string time_str;
  if (!time_to_string(t_now, &time_str))
    return false;
  vse_clause* cn = new(vse_clause);
  cn->CopyFrom(cl);
  at.set_allocated_clause(cn);
  at.set_time(time_str);
  string serialized;
  at.SerializeToString(serialized_attestation);
  return true;
}

// -----------------------------------------------------------------------

void print_storage_info(const storage_info_message& smi) {
  printf("\nStorage info:\n");
  if (smi.has_storage_type())
    printf("Storage type: %s\n", smi.storage_type().c_str());
  if (smi.has_storage_descriptor())
    printf("Storage descriptor: %s\n", smi.storage_descriptor().c_str());
  if (smi.has_address())
    printf("address: %s\n", smi.address().c_str());
  if (smi.has_storage_key())
    print_key(smi.storage_key());
}

void print_trusted_service_message(const trusted_service_message& tsm) {
  printf("\nTrusted service\n");
  if (tsm.has_trusted_service_address())
    printf("Service address: %s\n", tsm.trusted_service_address().c_str());
  if (tsm.has_trusted_service_key())
    print_key(tsm.trusted_service_key());
}

void print_protected_blob(protected_blob_message& pb) {
  if (pb.has_encrypted_key()) {
    printf("encrypted_key (%d): ", (int)pb.encrypted_key().size());
    print_bytes((int)pb.encrypted_key().size(), (byte*)pb.encrypted_key().data());
    printf("\n");
  }
  if (pb.has_encrypted_data()) {
    printf("encrypted_data (%d): ", (int)pb.encrypted_data().size());
    print_bytes((int)pb.encrypted_data().size(), (byte*)pb.encrypted_data().data());
    printf("\n");
  }
}

// -----------------------------------------------------------------------

int add_ext(X509 *cert, int nid, const char *value) {
  X509_EXTENSION *ex;
  X509V3_CTX ctx;

  // This sets the 'context' of the extensions.
  X509V3_set_ctx_nodb(&ctx);

  X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
  ex = X509V3_EXT_conf_nid(NULL, &ctx, nid, value);
  if(!ex)
    return 0;

  X509_add_ext(cert,ex, -1);
  X509_EXTENSION_free(ex);
  return 1;
}

// Caller should have allocated X509
// name is some printable version of the measurement
bool produce_artifact(key_message& signing_key, string& issuer_name_str,
                      string& issuer_organization_str, key_message& subject_key,
                      string& subject_name_str, string& subject_organization_str,
                      uint64_t sn, double secs_duration, X509* x509, bool is_root) {

  ASN1_INTEGER* a = ASN1_INTEGER_new();
  ASN1_INTEGER_set_uint64(a, sn);
  X509_set_serialNumber(x509, a);

  X509_NAME* subject_name =  X509_NAME_new();
  X509_NAME_add_entry_by_txt(subject_name, "CN", MBSTRING_ASC,
                            (unsigned char *)subject_name_str.c_str(), -1, -1, 0);
  X509_NAME_add_entry_by_txt(subject_name, "O", MBSTRING_ASC,
                        (const byte*)subject_organization_str.c_str(), -1, -1, 0);
  X509_set_subject_name(x509, subject_name);

  X509_NAME* issuer_name =  X509_NAME_new();
  X509_NAME_add_entry_by_txt(issuer_name, "CN", MBSTRING_ASC,
                            (unsigned char *)issuer_name_str.c_str(), -1, -1, 0);
  X509_NAME_add_entry_by_txt(issuer_name, "O", MBSTRING_ASC,
                        (const byte*)issuer_organization_str.c_str(), -1, -1, 0);
  X509_set_issuer_name(x509, issuer_name);

  time_t t_start = time(NULL);
  ASN1_TIME* tm_start= ASN1_TIME_new();
  ASN1_TIME_set(tm_start, t_start);
  int offset_day = (int) (secs_duration / 86400.0);
  long offset_sec = ((long)secs_duration) - ((long)offset_day * 86400);
  ASN1_TIME* tm_end = ASN1_TIME_adj(NULL, t_start, offset_day, offset_sec);
  X509_set1_notBefore(x509, tm_start);
  X509_set1_notAfter(x509, tm_end);

  add_ext(x509, NID_key_usage, "critical,keyCertSign,digitalSignature,cRLSign");
  add_ext(x509,  NID_ext_key_usage, "clientAuth,serverAuth");
  // add_ext(x509, NID_subject_key_identifier, "hash");
  if (is_root) {
    add_ext(x509, NID_basic_constraints, "critical,CA:TRUE");
  }

  if (signing_key.key_type() == "rsa-1024-private" ||
      signing_key.key_type() == "rsa-2048-private" ||
      signing_key.key_type() == "rsa-4096-private") {
    RSA* signing_rsa_key = RSA_new();
    if (!key_to_RSA(signing_key, signing_rsa_key))
      return false;
    EVP_PKEY* signing_pkey = EVP_PKEY_new();
    EVP_PKEY_set1_RSA(signing_pkey, signing_rsa_key);
    X509_set_pubkey(x509, signing_pkey);

    RSA* subject_rsa_key = RSA_new();
    if (!key_to_RSA(subject_key, subject_rsa_key))
      return false;
    EVP_PKEY* subject_pkey = EVP_PKEY_new();
    EVP_PKEY_set1_RSA(subject_pkey, subject_rsa_key);
    X509_set_pubkey(x509, subject_pkey);
    if (signing_key.key_type() == "rsa-4096-private") {
      X509_sign(x509, signing_pkey, EVP_sha384());
    } else {
      X509_sign(x509, signing_pkey, EVP_sha256());
    }
    EVP_PKEY_free(signing_pkey);
    EVP_PKEY_free(subject_pkey);
    RSA_free(signing_rsa_key);
    RSA_free(subject_rsa_key);
  } else if (signing_key.key_type() == "ecc-384-private") {
    EC_KEY* signing_ecc_key = key_to_ECC(signing_key);
    if (signing_ecc_key == nullptr)
      return false;
    EVP_PKEY* signing_pkey = EVP_PKEY_new();
    EVP_PKEY_set1_EC_KEY(signing_pkey, signing_ecc_key);
    X509_set_pubkey(x509, signing_pkey);

    EC_KEY* subject_ecc_key = key_to_ECC(subject_key);
    if (subject_ecc_key == nullptr)
      return false;
    EVP_PKEY* subject_pkey = EVP_PKEY_new();
    EVP_PKEY_set1_EC_KEY(subject_pkey, subject_ecc_key);
    X509_set_pubkey(x509, subject_pkey);
    X509_sign(x509, signing_pkey, EVP_sha384());
    EVP_PKEY_free(signing_pkey);
    EVP_PKEY_free(subject_pkey);
    EC_KEY_free(signing_ecc_key);
    EC_KEY_free(subject_ecc_key);
  } else {
    printf("Unsupported algorithm\n");
    return false;
  }

  ASN1_INTEGER_free(a);
  ASN1_TIME_free(tm_start);
  ASN1_TIME_free(tm_end);
  X509_NAME_free(subject_name);
  X509_NAME_free(issuer_name);
  return true;
}

bool verify_artifact(X509& cert, key_message& verify_key, 
    string* issuer_name_str, string* issuer_description_str,
    key_message* subject_key, string* subject_name_str, string* subject_organization_str,
    uint64_t* sn) {

  bool success = false;
  if (verify_key.key_type() == "rsa-1024-public" ||
      verify_key.key_type() == "rsa-1024-private" ||
      verify_key.key_type() == "rsa-2048-private" ||
      verify_key.key_type() == "rsa-2048-private" ||
      verify_key.key_type() == "rsa-4096-private" ||
      verify_key.key_type() == "rsa-4096-private") {
    EVP_PKEY* verify_pkey = EVP_PKEY_new();
    RSA* verify_rsa_key = RSA_new();
    if (!key_to_RSA(verify_key, verify_rsa_key))
      return false;
    EVP_PKEY_set1_RSA(verify_pkey, verify_rsa_key);

    EVP_PKEY* subject_pkey = X509_get_pubkey(&cert);
    RSA* subject_rsa_key= EVP_PKEY_get1_RSA(subject_pkey);
    if (!RSA_to_key(subject_rsa_key, subject_key))
      return false;
    success = (X509_verify(&cert, verify_pkey) == 1);
    RSA_free(verify_rsa_key);
    RSA_free(subject_rsa_key);
    EVP_PKEY_free(verify_pkey);
    EVP_PKEY_free(subject_pkey);
  } else if (verify_key.key_type() == "ecc-384-public" ||
             verify_key.key_type() == "ecc-384-private") {
    EVP_PKEY* verify_pkey = EVP_PKEY_new();
    EC_KEY* verify_ecc_key = key_to_ECC(verify_key);
    if (verify_ecc_key == nullptr) 
      return false;
    EVP_PKEY_set1_EC_KEY(verify_pkey, verify_ecc_key);

    EVP_PKEY* subject_pkey = X509_get_pubkey(&cert);
    EC_KEY* subject_ecc_key= EVP_PKEY_get1_EC_KEY(subject_pkey);
    if (!ECC_to_key(subject_ecc_key, subject_key))
      return false;
    success = (X509_verify(&cert, verify_pkey) == 1);
    EC_KEY_free(verify_ecc_key);
    EC_KEY_free(subject_ecc_key);
    EVP_PKEY_free(verify_pkey);
    EVP_PKEY_free(subject_pkey);
  } else {
    printf("Unsupported key type\n");
    return false;
  }

  // Todo: report other cert values
  X509_NAME* subject_name = X509_get_subject_name(&cert);
  const int max_buf = 2048;
  char name_buf[max_buf];
  if (X509_NAME_get_text_by_NID(subject_name, NID_commonName, name_buf, max_buf) < 0)
    success = false;
  else {
    subject_name_str->assign((const char*) name_buf);
  }
 
  X509_NAME_free(subject_name);
  return success;
}

// -----------------------------------------------------------------------

bool asn1_to_x509(const string& in, X509 *x) {
  int len = in.size();

  byte* p = (byte*) in.data();
  d2i_X509(&x, (const byte**)&p, len);
  if (x == nullptr)
    return false;
  return true;
}

bool x509_to_asn1(X509 *x, string* out) {
  int len = i2d_X509(x, nullptr);
  byte buf[len];
  byte* p = buf;

  i2d_X509(x, (byte**)&p);
  out->assign((char*)buf, len);
  return true;
}

// Blocking read of pipe, socket, SSL connection when you
//  don't know the size of the buffer in advance

// This sucks but...
//    You can get max_pipe_size from fcntl(fd, F_GETPIPE_SZ);
const int max_pipe_size = 65536;
int sized_pipe_read(int fd, string* out) {
  out->clear();
  byte buf[max_pipe_size];
  int n = read(fd, buf, max_pipe_size);
  if (n < 0)
    return -1;
  out->assign((char*)buf, n);
  return n;
}

int sized_ssl_read(SSL* ssl, string* out) {
  out->clear();
#if 1
  int total = 0;
  const int read_stride = 8192;
  byte buf[read_stride];
  int n = 0;

  for (;;) {
    n = SSL_read(ssl, buf, read_stride);
    if (n < 0) {
      return n;
    } else if (n < read_stride) {
      out->append((char*)buf, n);
      total += n;
      break;
    } else {
      out->append((char*)buf, n);
      total += n;
      if (SSL_pending(ssl) <= 0)
        break;
      continue;
    }
  }
  return total;
#else
  byte buf[32000];
  int n = SSL_read(ssl, buf, 32000);
  if (n < 0)
    return -1;
  out->assign((char*)buf, n);
  return n;
#endif
}

// -----------------------------------------------------------------------

int sized_socket_read(int fd, string* out) {
  out->clear();
#if 1
  int n = 0;
  int total = 0;
  const int read_stride = 8192;
  byte buf[read_stride];
  int m = recv(fd, buf, read_stride, MSG_PEEK);
  while(m > 0) {
    n = read(fd, buf, read_stride);
    if (n <= 0) {
      return total;
    } else if (n < read_stride) {
      out->append((char*)buf, n);
      total += n;
      m -= n;
    } else {
      out->append((char*)buf, n);
      total += n;
      m -= n;
    }
  }
  return total;
#else
  byte buf[32000];
  int n = read(fd, buf, 32000);
  if (n < 0)
    return -1;
  out->assign((char*)buf, n);
  return n;
#endif
}

// -----------------------------------------------------------------------

// make a public key from the X509 cert
bool x509_to_public_key(X509* x, key_message* k) {
  EVP_PKEY* subject_pkey = X509_get_pubkey(x);
  if (subject_pkey == nullptr)
    return false;
  if (EVP_PKEY_base_id(subject_pkey) == EVP_PK_RSA) {
    int size = EVP_PKEY_bits(subject_pkey); 
    RSA* subject_rsa_key= EVP_PKEY_get1_RSA(subject_pkey);
    if (!RSA_to_key(subject_rsa_key, k))
      return false;
    switch(size) {
    case 1024:
      k->set_key_type("rsa-1024-public");
      break;
    case 2048:
      k->set_key_type("rsa-2048-public");
      break;
    case 4096:
      k->set_key_type("rsa-4096-public");
      break;
    default:
      return false;
    }
    // free subject_rsa_key?
  } else if (EVP_PKEY_base_id(subject_pkey) == EVP_PK_EC) {
    int size = EVP_PKEY_bits(subject_pkey); 
    EC_KEY* subject_ecc_key= EVP_PKEY_get1_EC_KEY(subject_pkey);
    if (!ECC_to_key(subject_ecc_key, k))
      return false;
    if (size == 384) {
      k->set_key_type("ecc-384-public");
    } else {
      return false;
    }
    // free subject_ecc_key?
  } else {
    return false;
  }

  X509_NAME* subject_name = X509_get_subject_name(x);
  const int max_buf = 2048;
  char name_buf[max_buf];
  if (X509_NAME_get_text_by_NID(subject_name, NID_commonName, name_buf, max_buf) < 0)
    return false;
  k->set_key_name((const char*) name_buf);
  k->set_key_format("vse-key");

  EVP_PKEY_free(subject_pkey);
  X509_NAME_free(subject_name);

  return true;
}

bool make_root_key_with_cert(string& type, string& name, string& issuer_name, key_message* k) {
  string root_name("root");

  if (type == "rsa-4096-private" || type == "rsa-2048-private" || type == "rsa-1024-private") {
    int n = 2048;
    if (type == "rsa-2048-private")
      n = 2048;
    else if (type == "rsa-1024-private")
      n = 1024;
    else if (type == "rsa-4096-private")
      n = 4096;

    if (!make_certifier_rsa_key(n,  k))
      return false;
    k->set_key_format("vse-key");
    k->set_key_type(type);
    k->set_key_name(name);
    double duration = 5.0 * 86400.0 * 365.0;
    X509* cert = X509_new();
    if (cert == nullptr)
      return false;
    if (!produce_artifact(*k, issuer_name, root_name, *k, issuer_name, root_name,
                      01L, duration, cert, true)) {
      return false;
    }
    string cert_asn;
    if (!x509_to_asn1(cert, &cert_asn))
      return false;
    k->set_certificate((byte*)cert_asn.data(), cert_asn.size());
    X509_free(cert);
  } else if (type == "ecc_384-private") {
    if (!make_certifier_ecc_key(384,  k))
      return false;
    k->set_key_format("vse-key");
    k->set_key_type(type);
    k->set_key_name(name);
    double duration = 5.0 * 86400.0 * 365.0;
    X509* cert = X509_new();
    if (cert == nullptr)
      return false;
    if (!produce_artifact(*k, issuer_name, root_name, *k, issuer_name, root_name,
                      01L, duration, cert, true)) {
      return false;
    }
    string cert_asn;
    if (!x509_to_asn1(cert, &cert_asn))
      return false;
    k->set_certificate((byte*)cert_asn.data(), cert_asn.size());
    X509_free(cert);
  } else {
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------


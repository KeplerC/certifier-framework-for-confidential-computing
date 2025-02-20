#include "certifier.h"
#include "support.h"

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

#ifdef SEV_SNP

extern bool verify_sev_Attest(int what_to_say_size, byte* what_to_say,
      int* size_measurement, byte* measurement,
      int size_report, byte* report_data);

bool test_sev(bool print_all) {
  const int data_size = 64;
  string enclave_type("sev-snp");
  string enclave_id("test-enclave");
  byte data[data_size];
  for (int i = 0; i < data_size; i++)
    data[i] = i;

  byte sealed[512];
  int sealed_size = 512;
  memset(sealed, 0, sealed_size);

  if (!Seal(enclave_type, enclave_id, data_size, data, &sealed_size, sealed))
    return false;

  if (print_all) {
    printf("\n");
    printf("data   size : %d\n", data_size);
    print_bytes(data_size, data);
    printf("\n");
    printf("Sealed size : %d\n", sealed_size);
    print_bytes(sealed_size, sealed);
    printf("\n");
  }

  byte unsealed[512];
  int unsealed_size = 512;
  memset(unsealed, 0, unsealed_size);

  if (!Unseal(enclave_type, enclave_id, sealed_size, sealed, &unsealed_size, unsealed))
    return false;

  if (print_all) {
    printf("\n");
    printf("unsealed size: %d\n", unsealed_size);
    print_bytes(unsealed_size, unsealed);
    printf("\n");
  }

  if (unsealed_size != data_size || memcmp(data, unsealed, data_size) != 0)
    return false;

  int what_to_say_size = 64;
  int size_measurement = 64;
  int size_out = 2048;
  byte what_to_say[what_to_say_size];
  byte measurement[size_measurement];
  byte out[size_out];

  memset(measurement, 0, size_measurement);
  memset(what_to_say, 0, what_to_say_size);
  memset(out, 0, size_out);

  const char* test_str = "I am a test string";
  what_to_say_size = strlen(test_str);
  memcpy(what_to_say, (byte*)test_str, what_to_say_size);

  if (!Attest(enclave_type, what_to_say_size, what_to_say, &size_out, out)) {
    printf("Attest failed\n");
    return false;
  }

  if (!verify_sev_Attest(what_to_say_size, what_to_say,
          &size_measurement, measurement, size_out, out)) {
    printf("Verify failed\n");
    return false;
  }

  if (print_all) {
    printf("\nMeasurement size: %d, measurement: ", size_measurement);
    print_bytes(size_measurement, measurement);
    printf("\n");
  }

  return true;
}
#endif

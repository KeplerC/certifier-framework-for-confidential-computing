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


bool test_seal(bool print_all) {
  string enclave_type("simulated-enclave");
  string enclave_id("local-machine");

  int secret_to_seal_size = 32;
  byte secret_to_seal[secret_to_seal_size];
  int sealed_size_out = 256;
  byte sealed[sealed_size_out];
  int recovered_size = 128;
  byte recovered[recovered_size];

  if (print_all) {
    printf("\nSeal\n");
    printf("to seal  (%d): ", secret_to_seal_size); print_bytes(secret_to_seal_size, secret_to_seal); printf("\n");
  }
  memset(sealed, 0, sealed_size_out);
  memset(recovered, 0, recovered_size);
  for (int i = 0; i < secret_to_seal_size; i++)
    secret_to_seal[i]= (7 * i)%16;

  if (!Seal(enclave_type, enclave_id, secret_to_seal_size, secret_to_seal, &sealed_size_out, sealed))
    return false;
  if (print_all) {
    printf("sealed   (%d): ", sealed_size_out); print_bytes(sealed_size_out, sealed); printf("\n");
  }

  if (!Unseal(enclave_type, enclave_id, sealed_size_out, sealed, &recovered_size, recovered))
    return false;

  if (print_all) {
    printf("recovered: (%d)", recovered_size); print_bytes(recovered_size, recovered); printf("\n");
  }
  return true;
}

bool test_attest(bool print_all) {
  string enclave_type("simulated-enclave");
  string enclave_id("test-enclave");
  string descript("simulated-test");

  key_message public_attestation_key;
  extern key_message my_attestation_key;

  if (!private_key_to_public_key(my_attestation_key, &public_attestation_key))
    return false;
  entity_message e1;
  entity_message e2;
  if (!make_key_entity(public_attestation_key, &e1))
    return false;

  extern string my_measurement;
  if (!make_measurement_entity(my_measurement, &e2))
    return false;
  string s1("says");
  string s2("speaks-for");
  vse_clause clause1;
  vse_clause clause2;
  if (!make_simple_vse_clause((const entity_message)e1, s2, (const entity_message)e2, &clause1))
    return false;
  if (!make_indirect_vse_clause((const entity_message)e1, s2, clause1, &clause2))
    return false;

  string serialized_attestation;
  if (!vse_attestation(descript, enclave_type, enclave_id,
        clause2, &serialized_attestation))
    return false;
  int size_out = 8192; 
  byte out[size_out]; 
  if (!Attest(enclave_type, serialized_attestation.size(),
        (byte*) serialized_attestation.data(), &size_out, out))
    return false;

  string ser_scm;
  signed_claim_message scm;
  ser_scm.assign((char*)out, size_out);
  if (!scm.ParseFromString(ser_scm))
    return false;
  vse_clause cl;
  return verify_signed_assertion_and_extract_clause(public_attestation_key, scm, &cl);
}


#include <gtest/gtest.h>
#include <gflags/gflags.h>

#include "support.h"
#include "certifier.h"
#include "simulated_enclave.h"
#include "cc_helpers.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include  <netdb.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/err.h>

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


// operations are: cold-init, warm-restart, get-certifier, run-app-as-client, run-app-as-server
DEFINE_bool(print_all, false,  "verbose");
DEFINE_string(operation, "", "operation");

DEFINE_string(client_address, "localhost", "address for client app");
DEFINE_string(server_address, "localhost", "address for server app");

DEFINE_string(policy_host, "localhost", "address for policy server");
DEFINE_int32(policy_port, 8123, "port for policy server");
DEFINE_string(data_dir, "./app1_data/", "directory for application data");

DEFINE_string(server_app_host, "localhost", "address for app server");
DEFINE_int32(server_app_port, 8124, "port for server app server");

DEFINE_string(policy_store_file, "store.bin", "policy store file name");
DEFINE_string(platform_file_name, "platform_file.bin", "platform certificate");
DEFINE_string(platform_attest_endorsement, "platform_attest_endorsement.bin", "platform endorsement of attest key");
DEFINE_string(attest_key_file, "attest_key_file.bin", "attest key");
DEFINE_string(measurement_file, "example_app.measurement", "measurement");


// The test app performs five possible roles
//    cold-init: This creates application keys and initializes the policy store.
//    warm-restart:  This retrieves the policy store data.
//    get-certifier: This obtains the app admission cert naming the public app key from the service.
//    run-app-as-client: This runs the app as a server.
//    run-app-as-server: This runs the app as a client

#include "policy_key.cc"
cc_trust_data* app_trust_data = nullptr;

// -----------------------------------------------------------------------------------------

void client_application(secure_authenticated_channel& channel) {

  printf("Client peer id is %s\n", channel.peer_id_.c_str());

  // client sends a message over authenticated, encrypted channel
  const char* msg = "Hi from your secret client\n";
  channel.write(strlen(msg), (byte*)msg);

  // Get server response over authenticated, encrypted channel and print it
  string out;
  int n = channel.read(&out);
  printf("SSL client read: %s\n", out.data());
}


void server_application(secure_authenticated_channel& channel) {

  printf("Server peer id is %s\n", channel.peer_id_.c_str());

  // Read message from client over authenticated, encrypted channel
  string out;
  int n = channel.read(&out);
  printf("SSL server read: %s\n", (const char*) out.data());

  // Reply over authenticated, encrypted channel
  const char* msg = "Hi from your secret server\n";
  channel.write(strlen(msg), (byte*)msg);
}

bool run_me_as_server(const string& host_name, int port,
      string& asn1_policy_cert, key_message& private_key,
      string& private_key_cert) {

  printf("running as server\n");
  server_dispatch(host_name, port, asn1_policy_cert, private_key,
      private_key_cert, server_application);
  return true;
}

int main(int an, char** av) {
  gflags::ParseCommandLineFlags(&an, &av, true);
  an = 1;
  ::testing::InitGoogleTest(&an, av);

  if (FLAGS_operation == "") {
    printf("example_app.exe --print_all=true|false --operation=op --policy_host=policy-host-address --policy_port=policy-host-port\n");
    printf("\t --data_dir=-directory-for-app-data --server_app_host=my-server-host-address --server_app_port=server-host-port\n");
    printf("\t --policy_cert_file=self-signed-policy-cert-file-name --policy_store_file=policy-store-file-name\n");
    printf("Operations are: cold-init, warm-restart, get-certifier, run-app-as-client, run-app-as-server\n");
    return 0;
  }

  SSL_library_init();
  string enclave_type("simulated-enclave");
  string purpose("authentication");

  string store_file(FLAGS_data_dir);
  store_file.append(FLAGS_policy_store_file);
  app_trust_data = new cc_trust_data(enclave_type, purpose, store_file);
  if (app_trust_data == nullptr) {
    printf("couldn't initialize trust object\n");
    return 1;
  }

  // Init policy key info
  if (!app_trust_data->init_policy_key(initialized_cert_size, initialized_cert)) {
    printf("Can't init policy key\n");
    return 1;
  }

  // Init simulated enclave
  string attest_key_file_name(FLAGS_data_dir);
  attest_key_file_name.append(FLAGS_attest_key_file);
  string platform_attest_file_name(FLAGS_data_dir);
  platform_attest_file_name.append(FLAGS_platform_attest_endorsement);
  string measurement_file_name(FLAGS_data_dir);
  measurement_file_name.append(FLAGS_measurement_file);
  string attest_endorsement_file_name(FLAGS_data_dir);
  attest_endorsement_file_name.append(FLAGS_platform_attest_endorsement);

  if (!app_trust_data->initialize_simulated_enclave_data(attest_key_file_name,
      measurement_file_name, attest_endorsement_file_name)) {
    printf("Can't init simulated enclave\n");
    return 1;
  }

  // Standard algorithms for the enclave
  string public_key_alg("rsa-2048");
  string symmetric_key_alg("aes-256");;
  string hash_alg("sha-256");
  string hmac_alg("sha-256-hmac");

  // Carry out operation
  int ret = 0;
  if (FLAGS_operation == "cold-init") {
    if (!app_trust_data->cold_init(public_key_alg,
        symmetric_key_alg, hash_alg, hmac_alg)) {
      printf("cold-init failed\n");
      ret = 1;
    }
  } else if (FLAGS_operation == "warm-restart") {
    if (!app_trust_data->warm_restart()) {
      printf("warm-restart failed\n");
      ret = 1;
    }

  } else if (FLAGS_operation == "get-certifier") {
    if (!app_trust_data->certify_me(FLAGS_policy_host, FLAGS_policy_port)) {
      printf("certification failed\n");
      ret = 1;
    }
  } else if (FLAGS_operation == "run-app-as-client") {
    if (!app_trust_data->warm_restart()) {
      printf("warm-restart failed\n");
      ret = 1;
      goto done;
    }
    printf("running as client\n");
    if (!app_trust_data->cc_auth_key_initialized_ ||
        !app_trust_data->cc_policy_info_initialized_) {
      printf("trust data not initialized\n");
      ret = 1;
      goto done;
    }
    string my_role("client");
    secure_authenticated_channel channel(my_role);
    if (!channel.init_client_ssl(host_name, port,
          app_trust_data->serialized_policy_cert_,
          app_trust_data->private_auth_key_,
          app_trust_data->private_auth_key_->certificate())) {
      printf("Can't init client app\n");
      ret = 1;
      goto done;
    }

  // This is the actual application code.
  client_application(channel);
  } else if (FLAGS_operation == "run-app-as-server") {
    if (!app_trust_data->warm_restart()) {
      printf("warm-restart failed\n");
      ret = 1;
      goto done;
    }
    printf("running as server\n");
    server_dispatch(host_name, port,
        app_trust_data->serialized_policy_cert_,
          app_trust_data->private_auth_key_,
          app_trust_data->private_auth_key_->certificate(),
          server_application);
  } else {
    printf("Unknown operation\n");
  }

done:
  // app_trust_data->print_trust_data();
  app_trust_data->clear_sensitive_data();
  if (app_trust_data != nullptr) {
    delete app_trust_data;
  }
  return ret;
}

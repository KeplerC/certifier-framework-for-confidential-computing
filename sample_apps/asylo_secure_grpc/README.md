# Certifier Asylo setup

```
git clone 
https://github.com/vmware-research/certifier-framework-for-confidential-computing.git
```

1. Setup Asylo
Copy Asylo repo to one directory above certifier to work around WORKSPACE and
bazel issues.
```
cd certifier-framework-for-confidential-computing
mv asylo ../
```

Install prerequisites
```
sudo apt install bison build-essential flex libisl-dev libmpc-dev \
        libmpfr-dev rsync texinfo wget zlib1g-dev
sudo apt install ocaml-nox ocamlbuild python
```

Install toolchain
```
cd asylo/distrib/toolchain
./install-toolchain --user --prefix="${HOME}"/toolchains/default
```

Install bazel
```
sudo apt install curl gnupg
curl https://bazel.build/bazel-release.pub.gpg | sudo apt-key add -
echo "deb http://storage.googleapis.com/bazel-apt stable jdk1.8" | \
    sudo tee /etc/apt/sources.list.d/bazel.list
sudo apt update
sudo apt install bazel
```

If tests have to be run on SGX hardware:
*   build and install the
    [Linux SGX driver](https://github.com/intel/linux-sgx-driver),
*   build and install the
    [plaform software (PSW)](https://github.com/intel/linux-sgx/blob/master/README.md#install-the-intelr-sgx-psw),
    and
*   [start the Architectural Enclave Service Manager](https://github.com/intel/linux-sgx#start-or-stop-aesmd-service).


2. Setup Certifier (Use "script" file to help with commands below)
```
export CERTIFIER=<certifier_base_repo_directory>
export CERTIFIER_PROTOTYPE=$CERTIFIER
export EXAMPLE_DIR=$CERTIFIER_PROTOTYPE/sample_apps/asylo_secure_grpc
export PATH=$PATH:/usr/local/go/bin && export PATH=$PATH:$(go env GOPATH)/bin

cd $CERTIFIER_PROTOTYPE/src
make -f certifier.mak

cd $CERTIFIER_PROTOTYPE/utilities
make -f cert_utility.mak
make -f policy_utilities.mak
```

Compile Certifier Protobuf in system protobuf and not Asylo protobuf
```
cd certifier-framework-for-confidential-computing/src 
protoc --cpp_out=. certifier.proto
cp certifier.pb.h ../include
cp certifier.pb.cc ../sample_apps/asylo_secure_grpc/
```

Create keys
```
mkdir -p $EXAMPLE_DIR/provisioning
cd $EXAMPLE_DIR/provisioning

$CERTIFIER_PROTOTYPE/utilities/cert_utility.exe --operation=generate-policy-key-and-test-keys \
    --policy_key_output_file=policy_key_file.bin --policy_cert_output_file=policy_cert_file.bin \
    --platform_key_output_file=platform_key_file.bin --attest_key_output_file=attest_key_file.bin
```

Embed policy key and compile app
```
cd $EXAMPLE_DIR/provisioning
$CERTIFIER_PROTOTYPE/utilities/embed_policy_key.exe --input=policy_cert_file.bin --output=../../include/policy_key.cc
```

Compile Asylo GRPC server (remove sgx_sim and add sgx_hw for hardware test) 
```
cd $CERTIFIER_PROTOTYPE/../asylo
bazel build //asylo/examples/secure_grpc:grpc_server_sgx_sim
```

Build policy
Find grpc_server_enclave_sgx_sim.so in ~/.cache/bazel/../ and add to input in the command below
```
cd $EXAMPLE_DIR/provisioning
$CERTIFIER_PROTOTYPE/utilities/measurement_utility.exe --type=hash --input=<bazel_build_path>/grpc_server_enclave_sgx_sim.so       --output=example_app.measurement

$CERTIFIER_PROTOTYPE/utilities/make_unary_vse_clause.exe --key_subject="" \
  --measurement_subject=example_app.measurement --verb="is-trusted" \
  --output=ts1.bin
$CERTIFIER_PROTOTYPE/utilities/make_indirect_vse_clause.exe --key_subject=policy_key_file.bin \
  --verb="says" --clause=ts1.bin --output=vse_policy1.bin

$CERTIFIER_PROTOTYPE/utilities/make_unary_vse_clause.exe --key_subject=platform_key_file.bin \
  --verb="is-trusted-for-attestation" --output=ts2.bin
$CERTIFIER_PROTOTYPE/utilities/make_indirect_vse_clause.exe --key_subject=policy_key_file.bin \
  --verb="says" --clause=ts2.bin --output=vse_policy2.bin

$CERTIFIER_PROTOTYPE/utilities/make_signed_claim_from_vse_clause.exe \
  --vse_file=vse_policy1.bin --duration=9000 \
  --private_key_file=policy_key_file.bin --output=signed_claim_1.bin
$CERTIFIER_PROTOTYPE/utilities/make_signed_claim_from_vse_clause.exe --vse_file=vse_policy2.bin \
  --duration=9000 --private_key_file=policy_key_file.bin --output=signed_claim_2.bin

$CERTIFIER_PROTOTYPE/utilities/package_claims.exe --input=signed_claim_1.bin,signed_claim_2.bin\
  --output=policy.bin

$CERTIFIER_PROTOTYPE/utilities/print_packaged_claims.exe --input=policy.bin

$CERTIFIER_PROTOTYPE/utilities/make_unary_vse_clause.exe --key_subject=attest_key_file.bin \
  --verb="is-trusted-for-attestation" --output=tsc1.bin
$CERTIFIER_PROTOTYPE/utilities/make_indirect_vse_clause.exe --key_subject=platform_key_file.bin \
  --verb="says" --clause=tsc1.bin --output=vse_policy3.bin
$CERTIFIER_PROTOTYPE/utilities/make_signed_claim_from_vse_clause.exe --vse_file=vse_policy3.bin \
  --duration=9000 --private_key_file=platform_key_file.bin \
  --output=platform_attest_endorsement.bin
$CERTIFIER_PROTOTYPE/utilities/print_signed_claim.exe --input=platform_attest_endorsement.bin
```

Provision service and apps
```
cd $EXAMPLE_DIR
mkdir -p client_data server_data
mkdir -p service

cd $EXAMPLE_DIR/provisioning

cp ./* $EXAMPLE_DIR/service
cp ./* $EXAMPLE_DIR/client_data
cp ./* $EXAMPLE_DIR/server_data
```

Compile the certifier service 

```
cd $CERTIFIER_PROTOTYPE/certifier_service
go build simpleserver.go
```

Run certifier service (in 1st window)
```
cd $EXAMPLE_DIR
$CERTIFIER_PROTOTYPE/certifier_service/simpleserver \
      --path=$EXAMPLE_DIR/service \
      --policyFile=policy.bin --readPolicy=true
```

Run GRPC Server (in 2nd window)
```
cd $CERTIFIER_PROTOTYPE/../asylo
bazel run //asylo/examples/secure_grpc:grpc_server_sgx_sim --  --acl="$(cat asylo/examples/secure_grpc/acl_isvprodid_2.textproto)"
```

Run GRPC Client (in 3rd window)
```
cd $CERTIFIER_PROTOTYPE/../asylo
bazel run //asylo/examples/secure_grpc:grpc_client_sgx_sim --   --word_to_translate="asylo"   --port=32455
```

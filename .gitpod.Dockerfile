FROM openms/contrib:latest
                    
#USER gitpod
# Avoid user input
ARG DEBIAN_FRONTEND=noninteractive

# Install tools for VSCode, Intellisense, JRE for Thirdparties, etc. using apt-get
RUN apt-get -q update && apt-get install -yq gdb wget clangd-9 php openjdk-11-jre clang-tidy && rm -rf /var/lib/apt/lists/*
#
# More information: https://www.gitpod.io/docs/42_config_docker/

#!/bin/bash

# Exit on error
set -e

# Path to your key file - replace with your actual key path
KEY_PAIR_NAME="p2pswarm"
SSH_USER="ec2-user"
PROJECT_DIR="/Users/lucaspeters/Documents/GitHub/P2PTorrenting"
REMOTE_DIR="~/"

# Files to copy
FILES_TO_COPY=(
  "docker-compose.bootstrap.yml"
  "bootstrap-compose.sh"
)

# Check if key file exists
if [ ! -f "${KEY_PAIR_NAME}.pem" ]; then
  echo "Error: SSH key file not found at ${KEY_PAIR_NAME}.pem"
  echo "Please ensure your AWS key is in the current directory"
  exit 1
fi

# Check if static_ips.txt exists
if [ ! -f "${PROJECT_DIR}/aws_scripts/static_ips.txt" ]; then
  echo "Error: static_ips.txt not found"
  exit 1
fi

# Read IPs from static_ips.txt
echo "Reading EC2 instance IPs from static_ips.txt..."
IPS=$(cat "${PROJECT_DIR}/aws_scripts/static_ips.txt")

# Loop through each IP
for IP in $IPS; do
  echo "===================================================="
  echo "Deploying to EC2 instance: $IP"
  echo "===================================================="
  echo "Copying setup script to instances..."
  for FILE in "${FILES_TO_COPY[@]}"; do 
    echo "Copying ${PROJECT_DIR}/aws_scripts/${FILE} to ${SSH_USER}@${IP}:~/${FILE}"
    ssh -i "${KEY_PAIR_NAME}.pem" -o StrictHostKeyChecking=no ${SSH_USER}@${IP} "rm -rf ${FILE}"
    scp -i "${KEY_PAIR_NAME}.pem" -o StrictHostKeyChecking=no "${PROJECT_DIR}/aws_scripts/${FILE}" ${SSH_USER}@${IP}:~/${FILE}
  done
  echo "Running bootstrap-compose.sh on ${IP}..."
done

for IP in $IPS; do
  ssh -i "${KEY_PAIR_NAME}.pem" -o StrictHostKeyChecking=no ${SSH_USER}@${IP} "chmod +x ~/bootstrap-compose.sh && ~/bootstrap-compose.sh"
done

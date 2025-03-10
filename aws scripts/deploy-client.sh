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
  #
  "${PROJECT_DIR}/aws scripts/bootstrap-compose.sh"
)

# Check if key file exists
if [ ! -f $(eval echo $KEY_FILE) ]; then
  echo "Error: SSH key file not found at $KEY_FILE"
  echo "Please update the KEY_FILE variable with the correct path to your AWS key"
  exit 1
fi

# Check if static_ips.txt exists
if [ ! -f "${PROJECT_DIR}/aws scripts/static_ips.txt" ]; then
  echo "Error: static_ips.txt not found"
  exit 1
fi

# Read IPs from static_ips.txt
echo "Reading EC2 instance IPs from static_ips.txt..."
IPS=$(cat "${PROJECT_DIR}/aws scripts/client_ips.txt")

# Loop through each IP
for IP in $IPS; do
  echo "===================================================="
  echo "Deploying to EC2 instance: $IP"
  echo "===================================================="
  echo "Copying setup script to instances..."
  scp -i "$KEY_PAIR_NAME.pem" -o StrictHostKeyChecking=no client-setup.sh ec2-user@$IP:~/client-setup.sh
  ssh -i "$KEY_PAIR_NAME.pem" -o StrictHostKeyChecking=no ec2-user@$IP "chmod +x ~/client-setup.sh && ~/client-setup.sh"
done
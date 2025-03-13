#!/bin/bash

# Exit on error
set -e

# Script for deploying P2P Torrenting application on Amazon Linux EC2
echo "P2P Torrenting Deployment Script for Amazon Linux EC2"
echo "======================================================"

# Update system packages
echo "Updating system packages..."
sudo yum update -y

# Install Docker if not already installed
if ! command -v docker &> /dev/null; then
    echo "Installing Docker..."
    sudo yum install -y docker
    sudo systemctl start docker
    sudo systemctl enable docker
    sudo usermod -aG docker ec2-user
    echo "Docker installed successfully!"
else
    echo "Docker is already installed."
fi

# Install Docker Compose if not already installed
if ! command -v docker-compose &> /dev/null; then
    echo "Installing Docker Compose..."
    sudo curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
    sudo chmod +x /usr/local/bin/docker-compose
    echo "Docker Compose installed successfully!"
else
    echo "Docker Compose is already installed."
fi

echo "Pulling latest image..."
sudo docker pull lucaspeters/client:latest

# Get the public IP of the EC2 instance
echo "Detecting public IP address..."
PUBLIC_IP=$(curl -s http://checkip.amazonaws.com)
echo "Public IP: $PUBLIC_IP"

# Export the PUBLIC_IP as an environment variable
export PUBLIC_IP

git clone https://github.com/lucas-peters/P2PTorrenting/ app
cd app
git submodule update --init --recursive
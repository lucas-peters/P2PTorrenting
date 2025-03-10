#!/bin/bash

# Exit on error
set -e

# Update package information
echo "Updating package information..."
sudo apt-get update

# Install Git
echo "Installing Git..."
sudo apt-get install -y git

# Install Docker
echo "Installing Docker..."
sudo apt-get install -y apt-transport-https ca-certificates curl software-properties-common
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io

# Start Docker service
echo "Starting Docker service..."
sudo systemctl start docker
sudo systemctl enable docker

# Install Docker Compose
echo "Installing Docker Compose..."
sudo curl -L "https://github.com/docker/compose/releases/download/v2.23.0/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose

# Add current user to the docker group to avoid using sudo with docker commands
echo "Adding current user to docker group..."
sudo usermod -aG docker $USER
newgrp docker

# Clone your repository
# Replace YOUR_REPO_URL with your actual repository URL
echo "Cloning your repository..."
git clone YOUR_REPO_URL
cd $(basename YOUR_REPO_URL .git)

# Run docker-compose
echo "Running docker-compose..."
docker-compose up -d

echo "Setup complete!"
#!/bin/bash
# Simplified script to set up P2P nodes on each EC2 instance

# Install docker-compose if not installed
if ! command -v docker-compose &> /dev/null; then
    echo "Installing docker-compose..."
    sudo curl -L "https://github.com/docker/compose/releases/download/v2.18.1/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
    sudo chmod +x /usr/local/bin/docker-compose
fi

# Get instance IP
PUBLIC_IP=$(curl -s http://169.254.169.254/latest/meta-data/public-ipv4)
echo "Instance public IP: $PUBLIC_IP"

# Clone the repository
if [ ! -d "~/p2p-swarm" ]; then
    echo "Cloning the repository..."
    git clone https://github.com/lucas-peters/P2PTorrenting/blob/main/Dockerfile.client ~/p2p-swarm
fi

# Create environment file
cd ~/p2p-swarm
cat > .env << EOL
PUBLIC_IP=$PUBLIC_IP
BOOTSTRAP_PORT=6881
INDEX_PORT=6882
CLIENT_PORT=6883
BOOTSTRAP_NODES=$PUBLIC_IP:6881  # Initially just this instance's bootstrap
INDEX_NODES=$PUBLIC_IP:6882      # Initially just this instance's index
EOL

# Start the bootstrap and index services
docker-compose up -d bootstrap index

echo "Bootstrap and Index nodes are now running."
echo "Bootstrap URL: $PUBLIC_IP:6881"
echo "Index URL: $PUBLIC_IP:6882"

# Create a file with node information
echo "Bootstrap: $PUBLIC_IP:6881" > node-info.txt
echo "Index: $PUBLIC_IP:6882" >> node-info.txt

echo "Setup complete!"

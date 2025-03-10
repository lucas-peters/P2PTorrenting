#!/bin/bash
# Script to deploy P2P torrent swarm nodes on AWS EC2

# Required: AWS CLI configuration with appropriate credentials

# Configuration variables
REGION="us-west-1"  # Change to your preferred AWS region
INSTANCE_TYPE="t2.micro"  # Change based on your requirements
KEY_PAIR_NAME="p2pswarm"  # Create or use an existing key pair
SECURITY_GROUP_NAME="p2p-swarm-sg"
AMI_ID="ami-01eb4eefd88522422"  # Amazon Linux 2 AMI (adjust for your region)
REPOSITORY_URL="https://github.com/lucas-peters/P2PTorrenting"  # Your Git repository URL

# Create security group if it doesn't exist
echo "Creating security group..."
aws ec2 create-security-group \
  --group-name $SECURITY_GROUP_NAME \
  --description "Security group for P2P torrent swarm" \
  --region $REGION 2>/dev/null || true

# Add inbound rules for your P2P application
aws ec2 authorize-security-group-ingress \
  --group-name $SECURITY_GROUP_NAME \
  --protocol tcp \
  --port 22 \
  --cidr 0.0.0.0/0 \
  --region $REGION 2>/dev/null || true

# Add rules for P2P ports: Bootstrap, Index, and related ports
for PORT in 6881 6882 6883 7881 7882 7883 8881 8882 8883; do
  aws ec2 authorize-security-group-ingress \
    --group-name $SECURITY_GROUP_NAME \
    --protocol tcp \
    --port $PORT \
    --cidr 0.0.0.0/0 \
    --region $REGION 2>/dev/null || true
done

for PORT in 6881 6882 6883 7881 7882 7883 8881 8882 8883; do
  aws ec2 authorize-security-group-ingress \
    --group-name $SECURITY_GROUP_NAME \
    --protocol udp \
    --port $PORT \
    --cidr 0.0.0.0/0 \
    --region $REGION 2>/dev/null || true
done

# Clear previous IP file if it exists
> static_ips.txt

# Create 5 instances
for i in {1..5}; do
  echo "Deploying instance $i..."
  
  # Create instance with Elastic IP
  INSTANCE_ID=$(aws ec2 run-instances \
    --image-id $AMI_ID \
    --count 1 \
    --instance-type $INSTANCE_TYPE \
    --key-name $KEY_PAIR_NAME \
    --security-groups $SECURITY_GROUP_NAME \
    --user-data "#!/bin/bash
      amazon-linux-extras install docker -y
      service docker start
      usermod -a -G docker ec2-user
      chkconfig docker on
      yum install -y git
      " \
    --tag-specifications "ResourceType=instance,Tags=[{Key=Name,Value=p2p-swarm-node-$i}]" \
    --region $REGION \
    --query "Instances[0].InstanceId" \
    --output text)
  
  echo "Instance $INSTANCE_ID created"
  
  # Wait for instance to be running
  aws ec2 wait instance-running --instance-ids $INSTANCE_ID --region $REGION
  
  # Allocate Elastic IP
  ALLOCATION_ID=$(aws ec2 allocate-address \
    --domain vpc \
    --region $REGION \
    --query AllocationId \
    --output text)
  
  # Associate Elastic IP with instance
  aws ec2 associate-address \
    --instance-id $INSTANCE_ID \
    --allocation-id $ALLOCATION_ID \
    --region $REGION
  
  # Get the public IP
  PUBLIC_IP=$(aws ec2 describe-addresses \
    --allocation-ids $ALLOCATION_ID \
    --query "Addresses[0].PublicIp" \
    --region $REGION \
    --output text)
  
  echo "Instance $i deployed with static IP: $PUBLIC_IP"
  
  # Store IPs for later use
  echo "$PUBLIC_IP" >> static_ips.txt
done

echo "All instances deployed. Static IPs saved to static_ips.txt"
echo "Waiting 60 seconds for instances to fully initialize..."
sleep 60

# Copy setup scripts to each instance
echo "Copying setup script to instances..."
for IP in $(cat static_ips.txt); do
  scp -i "$KEY_PAIR_NAME.pem" -o StrictHostKeyChecking=no setup.sh ec2-user@$IP:~/setup.sh
  ssh -i "$KEY_PAIR_NAME.pem" -o StrictHostKeyChecking=no ec2-user@$IP "chmod +x ~/setup.sh && ~/setup.sh"
done

echo "Initial setup complete on all instances."
echo "Run the network configuration script to connect all nodes together."

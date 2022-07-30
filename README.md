#  Over-the-Air (OTA) ML Model Updates to Edge Devices using Enhanced Chameleon Hash Keychain Capstone Project

## Installation

#### C libraries set-up for Chameleonhash.c Program adapted from the [GPS signal authentication program](https://link.springer.com/chapter/10.1007/978-3-030-93511-5_10)
`sudo apt-get install libssl-dev` #this working \
`sudo apt-get install libcurl4-openssl-dev` #this working \
`sudo apt install curl` \
`sudo apt install gcc` 

chameleonhash.c can be compiled using the `Makefile`

#### NodeJS set-up for Server 
`sudo apt install node` \
`npm install` 

#### Python Package set-up for Client (run from client directory)
`pip install -r requirements.txt` \
`pip install tinydb` \
`pip install requests` 

## Run the Program

#### Run the Server with the following node command
```
node server.js
```
#### Run the Client with the following command from the client directory 
```
python3 client.py
```
# Project Structure

- **server.js** : This contains the client program and all the server functionality.
- **/client** : This folder contains client-related code.
- **/client/client.py** : This contains the client program and all the client functionality.
- **/client/database** : This contains the client database in json format.
- **/sampleFile** : This folder contains various .hdf5 and .h sample files for testing purposes.
- **/postman API** : This folder contains postman API collection for testing purposes.
- **Makefile** : Makefile to compile the chameleonhash.c program.
- **chameleonhash** : Compiled chameleonhash C program.
- **chameleonhash.c** : Chameleonhash keychain functions to be execute from server and client program.
- **package-lock.json** : Automatically generated from npm.
- **package.json** : This contains the dependancies information to be install when we run `npm install`.
- **test.sh** - A script file to test run multiple clients at once.

# Links / References 
Chameleonhash.c Program adapted from: [GPS Signal Authentication Using a Chameleon Hash Keychain](https://link.springer.com/chapter/10.1007/978-3-030-93511-5_10) \
Install Node.js on Ubuntu 18.04: [Node.js](https://www.digitalocean.com/community/tutorials/how-to-install-node-js-on-ubuntu-18-04) \
Install Python packages with pip and requirements.txt: [requirements.txt](https://note.nkmk.me/en/python-pip-install-requirements/) 

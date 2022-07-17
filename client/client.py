import time
import requests
import json
from datetime import datetime
# module to run chamelonhash program
from subprocess import PIPE, run
# tinydb database to store the data
from tinydb import TinyDB, Query
import hashlib
import hmac
import binascii
import base64
import os
import sys 

db = TinyDB('./database/clientdb.json', sort_keys=False, indent=4, separators=(',', ': '))

# API to call
startGetKpervious_URL = "http://localhost:8080/api/getK_pervious" # call only once first K0
getK1andRprime_URL = "http://localhost:8080/api/getK1andRprime"
getHMACandVi_URL = "http://localhost:8080/api/getHMACandVi"
getTest_URL = 'http://localhost:8080/'

database = "./database/clientdb.json"

# global variable
k0 = None #  the first k
hmacMessage = None
v_i = None # v_latest
k1 = None # k_latest
rPrime = None
versionId = None
fileName = None

# call this only once K_0
def getK_pervious():
    global k0
    response  = requests.get(startGetKpervious_URL)
    k0 = response.text
    print("Getting first key from server: ")
    print("First key: " + k0 + "\n")
    insertFirstKey()

def getK1andRprime():
    global k1, rPrime
    response  = requests.get(getK1andRprime_URL)
    data = json.loads(response.text)
    print("Getting K_latest and r' from server: ")
    # print(data)
    k1 = data["K_i"]
    print("K_latest: " + k1)

    rPrime = data["Rprime"]
    print("R': " + rPrime + "\n") 

def getHMACandVi():
    global hmacMessage, v_i, fileName, versionId
    response  = requests.get(getHMACandVi_URL)
    data = json.loads(response.text)
    print("Getting HMAC(Vi,Ki) and latest Vi from server: ")
    # print(data)
    hmacMessage = data["hmacMessage"]
    print("hmacMessage: " + hmacMessage)

    v_i = data["V_i"]
    print("v_i: " + v_i + "\n")

    # get file metadata 
    fileName = data["fileName"]
    versionId = data["versionID"]
    # print("fileName: " + fileName)
    # print("versionId: " + versionId)

# insert data into DB to keep track of the data received from the server
def insertData():
    global hmacMessage, v_i, k1, rPrime, versionId, fileName

    insert_new = {
        'hmacMessage': hmacMessage,
        'v_i': v_i,
        'key': k1,
        'r_prime': rPrime,
        'fileName': fileName,
        'versionId': versionId,
        'timestamp': str(datetime.now())
    }
    print("Inserting data into database")
    time.sleep(1)
    db.insert(insert_new)
    print("Updated db" + "\n")
    

def insertFirstKey():
    global k0

    insert_new = {
        'key': k0,
        'timestamp': str(datetime.now())
    }
    db.insert(insert_new)
    print("Updated database with first key" + "\n")


# vaildate K1 with K0 = CH (K1 || V1, r')
def validateCH(v_i, k1, rPrime):

    # reterive K_previous from db to verfiy K0 = CH (K1 || V1, r')
    # getK_pervious = db.get(doc_id=len(db)-1)
    getK_pervious = db.get(doc_id=len(db))
    k_previous = getK_pervious["key"]
    print("k_previous: " + k_previous)
    concatK1andV1 = k1+v_i
    print("concatK1andV1: " + concatK1andV1)

    command = ['../chameleonhash', '2', concatK1andV1, rPrime]
    result = run(command, stdout=PIPE, stderr=PIPE, universal_newlines=True)
    result_toVerify = str(result.stdout)
    print("CH from c program: " + result_toVerify + '\n')

    print("Verifying chameleon hash value ...")
    # time.sleep(2)
    # print(k_previous)
    # print(result_toVerify)
    checkString(k_previous, result_toVerify)

def checkString(k_previous, result_toVerify):
    # print(type(k_previous))
    # print(type(result_toVerify))
    if k_previous == result_toVerify:
        print("Chameleon hash verification is successful!\n")
    elif k_previous != result_toVerify:
        print(k_previous)
        print(result_toVerify)
        print("Chameleon hash verification is unsuccessful!\n")
        sys.exit("EXITING PROGRAM")
    

# decode base64 data and write to file
def decodeBase64(encodedBase64_string):
    decoded = base64.b64decode(encodedBase64_string)
    with open('newModel.h', 'w', encoding="utf-8") as output_file:
        output_file.write(decoded.decode("utf-8"))
        print("Updating new model")
        print("base64 data decoded and new model updated! \n")

# do rollback to previous version - tbc
def getVersion():
    getVer = Query()
    result = db.search(getVer.timestamp == "2022-07-15 03:43:22.893210")
    print(result)

# list all the past update history from db
def getAlldb():
    getAll_data = db.all()
    # Pretty Print JSON
    json_formatted_str = json.dumps(getAll_data, indent=4)
    print(json_formatted_str)

# Verify HMAC Value
def verifyHMAC(v_i, k1, hmacMessage):
    byte_key = bytes(k1, 'UTF-8')
    message = v_i.encode()
    hmac_toVerify = hmac.new(byte_key, message, hashlib.sha256).hexdigest()
    # print(hmac_toVerify)
    print("Verifying HMAC value sent from server ...")
    time.sleep(1)
    if hmac_toVerify == hmacMessage:
        print("HMAC verification is successful!\n")
        decodeBase64(v_i)
    else:
        print("HMAC verification is unsuccessful!\n")
        sys.exit("EXITING PROGRAM")

# check if V_latest from db is the same with local db
def checkViwithServerDB(v_i):
    getV_lastest = db.get(doc_id=len(db))
    v_latest = getV_lastest["v_i"]
    print("v_latest: " + v_latest + "\n" )
    if v_latest == v_i:
        print("Model is up to date with server\n")
        return True
    else:
        print("Model is not up to date\n")
        return False

def main():
    # db.insert({"k_previous" : 0})
    # check if there is any record in db
    if os.stat(database).st_size == 0:
        print('database is empty, getting the first Key from Server')
        time.sleep(2)
        getK_pervious()

        if k0 != None:
            # print('database is not empty')
            getHMACandVi()
            time.sleep(2)
            if hmacMessage != None and v_i != None:
                getK1andRprime()
                time.sleep(2)
                if k1 != None and rPrime != None:
                    time.sleep(2)
                    validateCH(v_i, k1, rPrime)
                    verifyHMAC(v_i, k1, hmacMessage)
                    insertData()
                else:
                    print("An error has occurred")
            else:
                print("An error has occurred")
    # if there is existing record in db 
    elif os.stat(database).st_size != 0:
        print("There is existing record in db\n")
        # retrive V_latest from db and check if there is any new update
        getHMACandVi()
        time.sleep(2)
        
        """
        check if V_latest from server db is the same with local db, if same then the model is up to date
        if not the same V_latest model from server then get new K_latest and r'
        """
        if checkViwithServerDB(v_i) == False:
            getK1andRprime()
            time.sleep(2)
            if k1 != None and rPrime != None:
                time.sleep(2)
                validateCH(v_i, k1, rPrime)
                verifyHMAC(v_i, k1, hmacMessage)
                insertData()
            else:
                print("An error has occurred")
        else:
            print("EXITING PROGRAM")
    else:
        print("An error has occurred") 

if __name__ == "__main__":
    main()

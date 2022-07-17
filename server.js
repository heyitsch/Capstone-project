const {
    exec
} = require("child_process");
const express = require('express');
const crypto = require('crypto');
const app = express();

// Calling the express.json() method for parsing
app.use(express.json());

//fs module 
const fs = require('fs');

// set default timezone 
process.env.TZ = 'Asia/Singapore';

var program = "chameleonhash";
var database = "chameleonhashDB";

// mongodb collection 
var dataNew = "fileDataTest_3";
var Rprime = "Rprime_data"

var MongoClient = require('mongodb').MongoClient;

var url = "mongodb+srv://ch:hag88viJgv8JFQLh@ota-chameleon-hash.qg0p1tv.mongodb.net/?retryWrites=true&w=majority";

// options to ensure that the connection is done properly
var mongoOptions = {
    useNewUrlParser: true,
    useUnifiedTopology: true
};

// watch for new model uploaded to db
MongoClient.connect(url, mongoOptions, function (err, db) {
    if (err) throw err;
    var coll = db.db(database).collection(dataNew);
    const changeStream = coll.watch();
    changeStream.on('change', c => {
        console.log('New model file uploaded to db:','\n', 'New model file name: ', c.fullDocument.fileName, '\n', 'CH Key_i: ', c.fullDocument.key, '\n', 'Message V_i: ', c.fullDocument.message, '\n', 'Random No: ', c.fullDocument.random, '\n');
       
    });
});

app.get('/', (req, res) => {
    res.send('Chameleon Hash Protocol REST API');
});

// insert file and CH key data to db - done
function insertDataNew(data) {
    MongoClient.connect(url, mongoOptions, function (err, db) {
        if (err) throw err;
        var dbo = db.db(database);
        dbo.collection(dataNew).insertOne(data, function (err, res) {
            if (err) throw err;
            console.log("Insert 1 document to  " + data);
            db.close();
            // generateNewRprime
            generateNewRprime() 
        });
    });
}

// insert r' to db
function insertRprimeNew(data) {
    MongoClient.connect(url, mongoOptions, function (err, db) {
        if (err) throw err;
        var dbo = db.db(database);
        dbo.collection(Rprime).insertOne(data, function (err, res) {
            if (err) throw err;
            console.log("Insert 1 document to RprimeNew DB " + data + '\n');
            db.close();
        });
    });
}

// const fileName = '/Users/joe/test.txt';
// Ki = CH(Vi,Ki) - K_latest = CH (V_latest, K_latest)
function generateChameleonHashforModel(file, fileName, versionID) {
    console.log("GENRATE NEW CH FOR NEW MODEL")
    var fileAsBase64 = fs.readFileSync(file, 'base64').toString();
    // var fileAsBase64 = fs.readFileSync(file, 'binary').toString();
    // console.log('fileAsBase64', '\n' + fileAsBase64)
    
    // Double quotes are used so that the space in the path is not interpreted as
    exec("./" + program + " 0 \"" + fileAsBase64 + "\"", (error, stdout, stderr) => {
        console.log("C: %s", stdout);
        if (!(stdout == "Fail")) {

            var values = stdout.trim().split(',');
            var obj = {
                key: values[0],
                random: values[1],
                fileName: fileName,
                versionId: versionID,
                message: fileAsBase64, // V_i
                hmacMessage: Hmac(fileAsBase64, values[0]).toString(),
                date: new Date().toLocaleString(),
                timestamp: new Date().getTime(),
            }
            insertDataNew(obj);
            console.log("New file model inserted into db")
        } else {
            res.send("fail");
            console.log("Fail to insert new model into db")
        }
    });

}

app.post('/api/generateChameleonHashforModel', (req, res) => {
    console.log("GENRATE NEW CH FOR NEW MODEL")
    console.log(req.body);
    var fileAsBase64 = fs.readFileSync(req.body.file, 'base64').toString();
    exec("./" + program + " 0 \"" + fileAsBase64 + "\"", (error, stdout, stderr) => {
        console.log("C: %s", stdout);
        if (!(stdout == "Fail")) {

            var values = stdout.trim().split(',');
            var obj = {
                key: values[0],
                random: values[1],
                fileName: req.body.fileName,
                versionId: req.body.versionID,
                message: fileAsBase64, // V_i
                hmacMessage: Hmac(fileAsBase64, values[0]).toString(),
                date: new Date().toLocaleString(),
                timestamp: new Date().getTime(),
            }
            insertDataNew(obj);

            res.send("success");
        } else {
            res.send("fail");
        }
    });
});

// reterive latest 2 inserted docs from db to compute r'
function generateNewRprime() {

    var resultTodb = {};

    MongoClient.connect(url, mongoOptions, function(err, db) {
        if (err) throw err;
        var dbo = db.db(database);
        //Find the first document in the customers collection:
         // find().skip(n).limit(1)
        dbo.collection(dataNew).find({}).sort({timestamp: -1})
        .limit(2).toArray(function(err, result) {
          if (err) throw err;
        //   console.log("Query\t%s\n", JSON.stringify(result));

          // k1 - latest updated file details
          console.log("Latest updated Key details k_latest")
          var key1 = result[0].key
          console.log("k_latest: " + key1)
          var v1 = result[0].message
          console.log("v_latest: " + v1)

          var concatK1andV1 = key1.concat(v1);
          console.log("k1||v1: " + concatK1andV1)

          // k_previous , r_previous
        //   console.log("mo and ro details")
          var random_0 = result[1].random;
          console.log("random_previous: " + random_0)
          var message_0 = result[1].message;
          console.log("v_previous: " + message_0)

          var randomPrime = null;

          exec("./" + program + " 1 " + random_0 + " \"" + message_0 + "\" " + concatK1andV1, (error, stdout, stderr) => {
            randomPrime = stdout.trim();
            console.log("randomPrime: " + randomPrime);

            resultTodb.key = key1
            resultTodb.concatK1andV1 = concatK1andV1
            resultTodb.randomPrime = randomPrime
            resultTodb.date = new Date().toLocaleString()
            resultTodb.timestamp = new Date().getTime()
            // console.log(resultTodb)

            // call insertRprimeNew db to send the latest r' to client later
            insertRprimeNew(resultTodb)

        });

          db.close();
        });

      });

}

// send k_pervious - to client - done
app.get('/api/getK_pervious', (req, res) => {

    var key0 

    MongoClient.connect(url, mongoOptions, function(err, db) {
        if (err) throw err;
        var dbo = db.db(database);
        //Find the first document in the customers collection:
         // find().skip(n).limit(1)
        dbo.collection(dataNew).find({}).sort({timestamp: -1})
        .limit(2).toArray(function(err, result) {
          if (err) throw err;
        //   console.log("Query for get Ki\t%s\n", JSON.stringify(result));

          // k0 - reterive k_pervious to send to client
          key0 = result[1].key
          console.log("Client requested K_pervious: " + key0 + "\n")

          res.send(key0)
         
          db.close();
        });

      })
        
});

// send HMAC(Vi,Ki) - HMAC(V_latest, K_latest) and Vi - V_latest to client - done
app.get('/api/getHMACandVi', (req, res) => {
    var dataToClient = {
        "hmacMessage": "",
        "V_i": "",
        "fileName": "",
        "versionID": "",
    }; 

    MongoClient.connect(url, mongoOptions, function(err, db) {
        if (err) throw err;
        var dbo = db.db(database);
        //Find the first document in the customers collection:
         // find().skip(n).limit(1)
        dbo.collection(dataNew).find({}).sort({timestamp: -1})
        .limit(1).toArray(function(err, result) {
          if (err) throw err;
          console.log("Client requested for HMAC(Vi,Ki) and Vi ")
          console.log("Query for HMAC(Vi,Ki) and Vi )\t%s\n", JSON.stringify(result));

          dataToClient.hmacMessage = result[0].hmacMessage
          console.log("HMAC Message: " + dataToClient.hmacMessage)

          dataToClient.V_i = result[0].message
          console.log("V_latest: " + dataToClient.V_i + "\n")

          // send file metadata to client 
          dataToClient.fileName = result[0].fileName
        //   console.log("fileName: " + dataToClient.fileName + "\n")

          dataToClient.versionID = result[0].versionId
        //   console.log("versionID: " + dataToClient.versionID + "\n")
        
          res.send(dataToClient)
         
          db.close();
        });

      })
        
});

// send K1 - K_latest and r' to client - done
app.get('/api/getK1andRprime', (req, res) => {
    var dataToClient = {
        "K_i": "",
        "Rprime": "",
    }; 

    MongoClient.connect(url, mongoOptions, function(err, db) {
        if (err) throw err;
        var dbo = db.db(database);
        //Find the first document in the customers collection:
         // find().skip(n).limit(1)
        dbo.collection(Rprime).find({}).sort({timestamp: -1})
        .limit(1).toArray(function(err, result) {
          if (err) throw err;
          console.log("Client requested for K_latest and r' ")
        //   console.log("Query for K_latest and r'\t%s\n", JSON.stringify(result));

          dataToClient.K_i = result[0].key
          console.log("K_latest: " +  dataToClient.K_i)

          dataToClient.Rprime = result[0].randomPrime
          console.log("r': " + dataToClient.Rprime + "\n")

          res.send(dataToClient)
         
          db.close();
        });

      })
        
});

// test k0 - upload new file for testing using function call
// generateChameleonHashforModel('./sampleFile/test.h', 'File test 5', '5.0')

// Return Hmac value
function Hmac(text, key) {
    return crypto.createHmac('sha256', key).update(text).digest('hex');
}

//PORT ENVIRONMENT VARIABLE
// use port 8080 unless there exists a preconfigured port
const port = process.env.PORT || 8080;
app.listen(port, () => console.log(`Server listenling on port ${port}..`));

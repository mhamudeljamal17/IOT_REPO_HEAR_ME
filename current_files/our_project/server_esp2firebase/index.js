const functions = require("firebase-functions");
const admin = require("firebase-admin");

admin.initializeApp();

exports.onNewDetection = functions.firestore
  .document("{detectID}")
  .onCreate(async (snap, context) => {

    const data = snap.data();

    const payload = {
      notification: {
        title: "New Detection",
        body: "Detection data added – tap to view",
      },
      data: {
        detectID: context.params.detectID,
        image: data.image?.stringValue || "",
        audio: data.audio?.stringValue || "",
      }
    };

    await admin.messaging().sendToTopic("detections", payload);
  });

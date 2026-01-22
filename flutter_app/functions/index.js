const {onDocumentWritten} = require("firebase-functions/v2/firestore");
const admin = require("firebase-admin");

admin.initializeApp();

exports.syncMenteeToRTDB = onDocumentWritten(
    "mentees/{docId}",
    async (event) => {
      const change = event.data;
      const after = change.after;
      const before = change.before;

      // If deleted → remove from RTDB
      if (!after.exists) {
        const oldData = before.data;
        if (oldData && oldData.menteeNumber) {
          await admin.database()
              .ref("/mentees/" + oldData.menteeNumber)
              .remove();
        }
        return;
      }

      const data = after.data;
      if (!data || !data.menteeNumber || !data.mentorId) return;

      const menteeNumber = data.menteeNumber.toString();

      await admin.database()
          .ref("/mentees/" + menteeNumber)
          .set({mentorId: data.mentorId});

      console.log("Synced mentee", menteeNumber);
    },
);

const {onDocumentWritten} = require("firebase-functions/v2/firestore");
const admin = require("firebase-admin");

admin.initializeApp();

// Sync mentee data to RTDB
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

      const data = after.data();
      if (!data || !data.menteeNumber || !data.mentorId) return;

      const menteeNumber = data.menteeNumber.toString();

      await admin.database()
          .ref("/mentees/" + menteeNumber)
          .set({mentorId: data.mentorId});

      console.log("✅ Synced mentee", menteeNumber);
    },
);

// NEW: Send FCM notification when mentor_alert is created
exports.sendMentorNotification = onDocumentWritten(
    "notifications_from_esp/{docId}",
    async (event) => {
      const change = event.data;
      const after = change.after;

      // Only process when document is created (not updated)
      if (!after.exists || (change.before.exists && after.exists)) {
        return;
      }

      const data = after.data();

      // Only process mentor_alert type notifications
      if (data.type !== "mentor_alert" && data.type !== "help_request") {
        return;
      }


      console.log("📬 Processing mentor alert:", after.id);

      try {
        const mentorId = data.mentorId;
        const menteeNumber = data.menteeNumber;

        if (!mentorId) {
          console.log("⚠️ No mentorId in notification");
          return;
        }

        // Get mentor's FCM token from users collection
        const mentorDoc = await admin.firestore()
            .collection("users")
            .doc(mentorId)
            .get();

        if (!mentorDoc.exists) {
          console.log("⚠️ Mentor user not found:", mentorId);
          return;
        }

        const mentorData = mentorDoc.data();
        const fcmToken = mentorData.fcmToken;

        if (!fcmToken) {
          console.log("⚠️ No FCM token for mentor:", mentorId);
          return;
        }

        // Send FCM notification
        const message = {
          token: fcmToken,
          notification: {
            title: data.title || "New Detection Alert",
            body: data.message || `Mentee #${menteeNumber} triggered detection`,
          },
          data: {
            menteeNumber: menteeNumber.toString(),
            mentorId: mentorId,
            imagePath: data.imagePath || "",
            audioPath: data.audioPath || "",
            timestamp: data.timestamp || new Date().toISOString(),
            type: "mentor_alert",
            notificationId: after.id,
          },
          android: {
            priority: "high",
            notification: {
              sound: "default",
              channelId: "hearme_channel",
              clickAction: "FLUTTER_NOTIFICATION_CLICK",
            },
          },
          apns: {
            payload: {
              aps: {
                alert: {
                  title: data.title || "New Detection Alert",
                  body:
    data.message ||
    `Mentee #${menteeNumber} triggered detection`,

                },
                sound: "default",
                badge: 1,
              },
            },
          },
          webpush: {
            notification: {
              title: data.title || "New Detection Alert",
              body:
    data.message ||
    `Mentee #${menteeNumber} triggered detection`,

              icon: "/icons/icon-192x192.png",
            },
          },
        };

        const response = await admin.messaging().send(message);
        console.log("✅ FCM sent to mentor:", mentorId, "Response:", response);

        // Create emergency record if emotion is angry or panic
        const emotion = data.emotion || "unknown";
        if (emotion === "angry" || emotion === "panic") {
          try {
            // Get mentee data
            const menteeQuery = await admin.firestore()
                .collection("mentees")
                .where("menteeNumber", "==", menteeNumber)
                .limit(1)
                .get();

            if (!menteeQuery.empty) {
              const menteeDoc = menteeQuery.docs[0];
              const menteeData = menteeDoc.data();

              // Create emergency record
              await admin.firestore().collection("emergencies").add({
                menteeId: menteeDoc.id,
                menteeName: menteeData.name || "Unknown",
                menteeNumber: menteeNumber,
                mentorId: mentorId,
                emotion: emotion,
                timestamp: admin.firestore.FieldValue.serverTimestamp(),
                audioPath: data.audioPath || null,
                imagePath: data.imagePath || null,
                status: "new",
              });

              console.log("✅ Emergency record created for mentee #" +
                menteeNumber);
            }
          } catch (emergencyError) {
            console.error("❌ Error creating emergency record:", emergencyError);
          }
        }

        // Update notification status
        await after.ref.update({
          status: "sent",
          fcmSentAt: admin.firestore.FieldValue.serverTimestamp(),
          fcmResponse: response,
        });
      } catch (error) {
        console.error("❌ Error sending FCM:", error);
        await after.ref.update({
          status: "failed",
          error: error.message,
          failedAt: admin.firestore.FieldValue.serverTimestamp(),
        });
      }
    },
);

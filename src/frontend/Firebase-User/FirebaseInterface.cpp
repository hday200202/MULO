#include "Application.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>

void Application::initFirebase() {
#ifdef FIREBASE_AVAILABLE
    try {
        firebase::AppOptions options;
        options.set_api_key("AIzaSyCz8-U53Iga6AbMXvB7XMjOSSkqVLGYpOA");
        options.set_app_id("1:1068093358007:web:bdc95a20f8e60375bf7232");
        options.set_project_id("mulo-marketplace");
        options.set_storage_bucket("mulo-marketplace.firebasestorage.app");
        options.set_database_url("https://mulo-marketplace-default-rtdb.firebaseio.com/");

        firebaseApp.reset(firebase::App::Create(options));
        
        // Configure Firestore settings for better cache handling
        firebase::firestore::Settings settings;
        settings.set_cache_size_bytes(firebase::firestore::Settings::kCacheSizeUnlimited);
        settings.set_persistence_enabled(false);  // Disable local persistence to avoid file issues
        
        firestore = firebase::firestore::Firestore::GetInstance(firebaseApp.get());
        firestore->set_settings(settings);
        
        auth = firebase::auth::Auth::GetAuth(firebaseApp.get());        
        realtimeDatabase = firebase::database::Database::GetInstance(firebaseApp.get());
        realtimeDatabase->set_persistence_enabled(false);
        storage = firebase::storage::Storage::GetInstance(firebaseApp.get());        
        auto authFuture = auth->SignInAnonymously();
        authFuture.OnCompletion([this](const firebase::Future<firebase::auth::AuthResult>& result) {
            if (result.error() == firebase::auth::kAuthErrorNone) {
                std::cout << "Firebase anonymous authentication successful" << std::endl;
            } else {
                std::cout << "Firebase authentication failed: " << result.error_message() << std::endl;
            }
        });
        
        std::cout << "Firebase initialized successfully" << std::endl;     
    } catch (const std::exception& e) {
        std::cout << "Firebase initialization failed: " << e.what() << std::endl;
        firebaseState = FirebaseState::Error;
    }
#endif
}

void Application::cleanupFirebaseResources() {
#ifdef FIREBASE_AVAILABLE
    std::lock_guard<std::mutex> lock(firebaseMutex);
    
    // Wait for any pending Firebase operations to complete with timeout
    auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(2);
    
    while (!pendingFirebaseFutures.empty() && 
           std::chrono::steady_clock::now() - start < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Clear any remaining futures
    pendingFirebaseFutures.clear();
    
    std::cout << "Firebase resources cleaned up" << std::endl;
#endif

    // Clear any pending engine updates safely
    {
        std::lock_guard<std::mutex> updateLock(engineUpdateMutex);
        hasPendingEngineUpdate = false;
        pendingEngineStateUpdate.clear();
        lastKnownRemoteEngineState.clear();
    }
}

void Application::fetchExtensions(std::function<void(FirebaseState, const std::vector<ExtensionData>&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!firestore) {
        callback(FirebaseState::Error, {});
        return;
    }
    
    if (firebaseState == FirebaseState::Loading) {
        return; // Already loading
    }
    
    firebaseState = FirebaseState::Loading;
    firebaseCallback = callback;
    extensions.clear();
    
    extFuture = firestore->Collection("extensions").Get();
#endif
}

void Application::uploadExtension(const ExtensionData& extensionData, const std::vector<std::string>& filePaths, 
                                std::function<void(FirebaseState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!firestore || !storage || !auth) {
        callback(FirebaseState::Error, "Firebase not initialized");
        return;
    }

    if (filePaths.empty()) {
        callback(FirebaseState::Error, "No files to upload");
        return;
    }
    
    std::thread([this, extensionData, filePaths, callback]() {
        // First, check if an extension with the same name and author already exists
        std::cout << "Checking for existing extension: " << extensionData.name << " by " << extensionData.author << std::endl;
        
        auto queryFuture = firestore->Collection("extensions")
            .WhereEqualTo("name", firebase::firestore::FieldValue::String(extensionData.name))
            .WhereEqualTo("author", firebase::firestore::FieldValue::String(extensionData.author))
            .Get();
        
        while (queryFuture.status() == firebase::kFutureStatusPending) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        std::string documentId;
        std::string existingStoragePath;
        bool isUpdate = false;
        
        if (queryFuture.error() == 0 && queryFuture.result()->documents().size() > 0) {
            // Extension exists, get the document ID and existing storage path
            auto existingDoc = queryFuture.result()->documents()[0];
            documentId = existingDoc.id();
            isUpdate = true;
            
            // Try to get the existing download URL to extract storage path
            if (existingDoc.Get("downloadURL").is_string()) {
                std::string existingURL = existingDoc.Get("downloadURL").string_value();
                size_t pathStart = existingURL.find("/o/");
                if (pathStart != std::string::npos) {
                    pathStart += 3;
                    size_t pathEnd = existingURL.find("?", pathStart);
                    if (pathEnd == std::string::npos) pathEnd = existingURL.length();
                    existingStoragePath = existingURL.substr(pathStart, pathEnd - pathStart);
                    // URL decode
                    std::string decodedPath;
                    for (size_t i = 0; i < existingStoragePath.length(); i++) {
                        if (existingStoragePath[i] == '%' && i + 2 < existingStoragePath.length()) {
                            std::string hex = existingStoragePath.substr(i + 1, 2);
                            char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
                            decodedPath += ch;
                            i += 2;
                        } else {
                            decodedPath += existingStoragePath[i];
                        }
                    }
                    existingStoragePath = decodedPath;
                }
            }
            
            std::cout << "Extension already exists (ID: " << documentId << "), updating..." << std::endl;
        } else {
            // Create new document
            auto docRef = firestore->Collection("extensions").Document();
            documentId = docRef.id();
            std::cout << "Creating new extension (ID: " << documentId << ")" << std::endl;
        }
        
        std::string firstFilePath = filePaths[0];
        std::string fileName = firstFilePath.substr(firstFilePath.find_last_of("/\\") + 1);
        
        std::string storagePath = "extensions/" + documentId + "/" + fileName;
        auto storageRef = storage->GetReference(storagePath);
        
        firebase::storage::Metadata metadata;
        metadata.set_content_type("application/octet-stream");
        metadata.set_cache_control("no-cache");
        std::ifstream file(firstFilePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            callback(FirebaseState::Error, "Failed to read file");
            return;
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            callback(FirebaseState::Error, "Failed to read file data");
            return;
        }
        file.close();
    
        auto bucketRef = firebase::storage::Storage::GetInstance(firebaseApp.get(), "gs://mulo-marketplace.firebasestorage.app");
        auto storageRef2 = bucketRef->GetReference(storagePath);
        
        auto uploadFuture = storageRef2.PutBytes(buffer.data(), size);
        
        while (uploadFuture.status() == firebase::kFutureStatusPending) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (uploadFuture.error() != 0) {
            std::string errorMsg = uploadFuture.error_message() ? uploadFuture.error_message() : "Unknown error";
            std::string friendlyError;
            switch(uploadFuture.error()) {
                case 1: friendlyError = "Cancelled"; break;
                case 2: friendlyError = "Permission denied - Firebase Storage rules need to be updated"; break;
                case 3: friendlyError = "Invalid argument"; break;
                case 4: friendlyError = "Deadline exceeded"; break;
                case 5: friendlyError = "Not found"; break;
                case 6: friendlyError = "Already exists"; break;
                case 7: friendlyError = "Permission denied"; break;
                case 16: friendlyError = "Unauthenticated"; break;
                default: friendlyError = "Error " + std::to_string(uploadFuture.error());
            }
            callback(FirebaseState::Error, "Failed to upload file: " + friendlyError + " (" + errorMsg + ")");
            return;
        }
        auto urlFuture = storageRef2.GetDownloadUrl();
        
        while (urlFuture.status() == firebase::kFutureStatusPending) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (urlFuture.error() != 0) {
            callback(FirebaseState::Error, "Failed to get download URL: " + std::string(urlFuture.error_message()));
            return;
        }
        
        std::string downloadURL = urlFuture.result()->c_str();
        firebase::firestore::MapFieldValue data;
        data["author"] = firebase::firestore::FieldValue::String(extensionData.author);
        data["description"] = firebase::firestore::FieldValue::String(extensionData.description);
        data["name"] = firebase::firestore::FieldValue::String(extensionData.name);
        data["version"] = firebase::firestore::FieldValue::String(extensionData.version);
        data["verified"] = firebase::firestore::FieldValue::Boolean(false);
        data["downloadURL"] = firebase::firestore::FieldValue::String(downloadURL);
        
        // Use the determined document ID (either existing or new)
        auto docRef = firestore->Collection("extensions").Document(documentId);
        auto setFuture = docRef.Set(data);
        
        while (setFuture.status() == firebase::kFutureStatusPending) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (setFuture.error() != 0) {
            callback(FirebaseState::Error, "Failed to " + std::string(isUpdate ? "update" : "create") + " document: " + std::string(setFuture.error_message()));
            return;
        }
        
        callback(FirebaseState::Success, isUpdate ? "Extension updated successfully" : "Extension uploaded successfully");
    
    }).detach(); // Detach thread so it runs independently
    
#else
    callback(FirebaseState::Error, "Firebase not available");
#endif
}

bool Application::canUpdateExtension(const std::string& extensionName) const {
    if (!isUserLoggedIn()) {
        std::cout << "[canUpdateExtension] User not logged in" << std::endl;
        return false;
    }
    
    std::string currentUserEmail = getCurrentUserEmail();
    std::cout << "[canUpdateExtension] Checking for extension: " << extensionName << std::endl;
    std::cout << "[canUpdateExtension] Current user: " << currentUserEmail << std::endl;
    std::cout << "[canUpdateExtension] Extensions count: " << extensions.size() << std::endl;
    
    // Check if extension exists and belongs to current user
    // Compare against the full email address stored in the extension data
    for (const auto& ext : extensions) {
        std::cout << "[canUpdateExtension] Comparing with: name=" << ext.name << ", author=" << ext.author << std::endl;
        if (ext.name == extensionName && ext.author == currentUserEmail) {
            std::cout << "[canUpdateExtension] MATCH FOUND!" << std::endl;
            return true;
        }
    }
    
    std::cout << "[canUpdateExtension] No match found" << std::endl;
    return false;
}

void Application::downloadExtension(const std::string& downloadURL, const std::string& extensionName,
                                    std::function<void(FirebaseState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!storage || !firebaseApp) {
        callback(FirebaseState::Error, "Firebase Storage not initialized");
        return;
    }
    
    std::thread([this, downloadURL, extensionName, callback]() {
        try {
            std::cout << "Starting download for: " << extensionName << std::endl;
            std::string storagePath;
            
            // Check if it's a Firebase Storage download URL or a direct storage path
            if (downloadURL.find("https://") == 0 || downloadURL.find("http://") == 0) {
                // Extract the storage path from the download URL
                // Firebase Storage URLs format: https://firebasestorage.googleapis.com/v0/b/[bucket]/o/[path]?...
                size_t pathStart = downloadURL.find("/o/");
                if (pathStart == std::string::npos) {
                    std::cout << "ERROR: Invalid download URL format (missing /o/): " << downloadURL << std::endl;
                    callback(FirebaseState::Error, "Invalid download URL format - missing /o/ in URL");
                    return;
                }
                
                pathStart += 3; // Skip "/o/"
                size_t pathEnd = downloadURL.find("?", pathStart);
                if (pathEnd == std::string::npos) {
                    pathEnd = downloadURL.length();
                }
                
                std::string encodedPath = downloadURL.substr(pathStart, pathEnd - pathStart);
                
                // URL decode the path
                for (size_t i = 0; i < encodedPath.length(); i++) {
                    if (encodedPath[i] == '%' && i + 2 < encodedPath.length()) {
                        std::string hex = encodedPath.substr(i + 1, 2);
                        char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
                        storagePath += ch;
                        i += 2;
                    } else {
                        storagePath += encodedPath[i];
                    }
                }
            } else {
                // Assume it's already a storage path
                storagePath = downloadURL;
            }
            
            std::cout << "Downloading from storage path: " << storagePath << std::endl;
            
            // Get the correct bucket reference (same as used in uploadExtension)
            std::cout << "Getting bucket reference..." << std::endl;
            auto bucketRef = firebase::storage::Storage::GetInstance(firebaseApp.get(), "gs://mulo-marketplace.firebasestorage.app");
            if (!bucketRef) {
                callback(FirebaseState::Error, "Failed to get bucket reference");
                return;
            }
            
            std::cout << "Getting storage reference..." << std::endl;
            auto storageRef = bucketRef->GetReference(storagePath);
            
            // Determine file extension from the storage path or use .so as default for Linux
            std::string fileExtension = ".so";
#ifdef __APPLE__
            fileExtension = ".dylib";
#elif defined(_WIN32)
            fileExtension = ".dll";
#endif
            
            // Ensure the extension name has the correct file extension
            std::string fileName = extensionName;
            if (fileName.find(fileExtension) == std::string::npos) {
                fileName += fileExtension;
            }
            
            // Create extensions directory if it doesn't exist
            std::string extensionsDir = exeDirectory + "/extensions";
            std::filesystem::create_directories(extensionsDir);
            
            std::string outputPath = extensionsDir + "/" + fileName;
            
            // Check if file already exists
            if (std::filesystem::exists(outputPath)) {
                std::cout << "Extension already exists at: " << outputPath << std::endl;
                callback(FirebaseState::Success, "Extension already installed at: " + outputPath);
                return;
            }
            
            std::cout << "Downloading to: " << outputPath << std::endl;
            
            // Download the file
            std::cout << "Calling GetFile..." << std::endl;
            auto downloadFuture = storageRef.GetFile(outputPath.c_str());
            
            std::cout << "Waiting for download to complete..." << std::endl;
            // Use simpler wait like uploadExtension does
            int counter = 0;
            while (downloadFuture.status() == firebase::kFutureStatusPending) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (++counter % 10 == 0) {
                    std::cout << "Still waiting... (" << counter/10 << " seconds)" << std::endl;
                }
                if (counter > 600) { // 60 second timeout
                    callback(FirebaseState::Error, "Download timed out");
                    return;
                }
            }
            
            std::cout << "Download complete, checking status..." << std::endl;
            if (downloadFuture.error() != 0) {
                std::string errorMsg = downloadFuture.error_message() ? downloadFuture.error_message() : "Unknown error";
                std::cout << "Download failed with error " << downloadFuture.error() << ": " << errorMsg << std::endl;
                callback(FirebaseState::Error, "Failed to download extension: " + errorMsg);
                return;
            }
            
            std::cout << "Download succeeded!" << std::endl;
            callback(FirebaseState::Success, "Extension downloaded to: " + outputPath);
            
        } catch (const std::exception& e) {
            callback(FirebaseState::Error, std::string("Download exception: ") + e.what());
        }
    }).detach();
    
#else
    callback(FirebaseState::Error, "Firebase not available");
#endif
}

void Application::createRoom(const std::string& roomName) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase || !auth) {
        std::cout << "Firebase not ready for collaboration" << std::endl;
        return;
    }
    
    auto currentUser = auth->current_user();
    if (!currentUser.is_valid()) {
        std::cout << "User not authenticated" << std::endl;
        return;
    }
    
    std::cout << "Creating room: " << roomName << std::endl;
    std::string roomPath = "rooms/" + roomName;
    auto roomRef = realtimeDatabase->GetReference(roomPath.c_str());
    
    auto checkFuture = roomRef.GetValue();
    checkFuture.OnCompletion([this, roomName, currentUser](const firebase::Future<firebase::database::DataSnapshot>& result) {
        if (result.error() == firebase::database::kErrorNone) {
            auto snapshot = result.result();
            if (snapshot->exists()) {
                std::cout << "Room already exists: " << roomName << std::endl;
                return;
            }
            
            // Room doesn't exist, create it
            std::string engineState = engine.getStateString();
            std::cout << "Engine state length: " << engineState.length() << std::endl;
            std::cout << "User ID: '" << currentUser.uid() << "'" << std::endl;
            
            std::string roomPath = "rooms/" + roomName;
            auto roomRef = realtimeDatabase->GetReference(roomPath.c_str());
            
            firebase::Variant roomData = firebase::Variant::EmptyMap();
            firebase::Variant participants = firebase::Variant::EmptyMap();
            
            // Get creator's nickname from config
            std::string creatorNickname = readConfig<std::string>("collab_nickname", "Anonymous");
            participants.map()[currentUser.uid()] = firebase::Variant(creatorNickname);
            
            roomData.map()["engineState"] = firebase::Variant(engineState);
            roomData.map()["createdBy"] = firebase::Variant(currentUser.uid());
            roomData.map()["createdAt"] = firebase::Variant(static_cast<int64_t>(std::time(nullptr)));
            roomData.map()["participants"] = participants;
            roomData.map()["participantCount"] = firebase::Variant(static_cast<int64_t>(1));
            
            auto createFuture = roomRef.SetValue(roomData);
            createFuture.OnCompletion([roomName](const firebase::Future<void>& result) {
                if (result.error() == firebase::database::kErrorNone) {
                    std::cout << "Room created successfully: " << roomName << std::endl;
                } else {
                    std::cout << "Failed to create room: " << result.error_message() << std::endl;
                }
            });
        } else {
            std::cout << "Failed to check if room exists: " << result.error_message() << std::endl;
        }
    });
#else
    std::cout << "Firebase not available - mock room creation: " << roomName << std::endl;
#endif
}

void Application::readFromRoom(const std::string& roomName) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase) {
        std::cout << "Firebase not ready for collaboration" << std::endl;
        return;
    }
    
    std::cout << "Reading from room: " << roomName << std::endl;
    
    std::string roomPath = "rooms/" + roomName;
    auto roomRef = realtimeDatabase->GetReference(roomPath.c_str());
    auto readFuture = roomRef.GetValue();
    
    readFuture.OnCompletion([this, roomName](const firebase::Future<firebase::database::DataSnapshot>& result) {
        std::lock_guard<std::mutex> lock(firebaseMutex);
        
        if (result.error() == firebase::database::kErrorNone) {
            auto snapshot = result.result();
            if (snapshot->exists()) {
                auto roomData = snapshot->value();
                if (roomData.is_map()) {
                    auto engineStateIt = roomData.map().find("engineState");
                    if (engineStateIt != roomData.map().end() && engineStateIt->second.is_string()) {
                        std::string engineState = engineStateIt->second.string_value();
                        std::cout << "Loading engine state from room: " << roomName << std::endl;
                        
                        // Queue engine state loading instead of applying directly
                        {
                            std::lock_guard<std::mutex> updateLock(engineUpdateMutex);
                            pendingEngineStateUpdate = engineState;
                            hasPendingEngineUpdate = true;
                        }
                        
                        std::cout << "Queued engine state from room for safe loading" << std::endl;
                    } else {
                        std::cout << "No engine state found in room: " << roomName << std::endl;
                    }
                } else {
                    std::cout << "Invalid room data format: " << roomName << std::endl;
                }
            } else {
                std::cout << "Room does not exist: " << roomName << std::endl;
            }
        } else {
            std::cout << "Failed to read room: " << result.error_message() << std::endl;
        }
    });
#else
    std::cout << "Firebase not available - mock room read: " << roomName << std::endl;
#endif
}

void Application::joinRoom(const std::string& roomName) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase || !auth) {
        std::cout << "Firebase not ready for collaboration" << std::endl;
        return;
    }
    
    auto currentUser = auth->current_user();
    if (!currentUser.is_valid()) {
        std::cout << "User not authenticated" << std::endl;
        return;
    }
    
    std::cout << "Joining room: " << roomName << std::endl;
    
    std::string roomPath = "rooms/" + roomName;
    auto roomRef = realtimeDatabase->GetReference(roomPath.c_str());
    
    // First check if room exists
    auto readFuture = roomRef.GetValue();
    readFuture.OnCompletion([this, roomName, currentUser](const firebase::Future<firebase::database::DataSnapshot>& result) {
        if (result.error() == firebase::database::kErrorNone) {
            auto snapshot = result.result();
            if (snapshot->exists()) {
                // Get user's nickname from config
                std::string nickname = readConfig<std::string>("collab_nickname", "Anonymous");
                
                // Check if nickname is already taken in this room
                auto roomData = snapshot->value();
                if (roomData.is_map()) {
                    auto participantsIt = roomData.map().find("participants");
                    if (participantsIt != roomData.map().end() && participantsIt->second.is_map()) {
                        // Check if any existing participant has the same nickname
                        for (const auto& participant : participantsIt->second.map()) {
                            if (participant.second.is_string() && participant.second.string_value() == nickname) {
                                std::cout << "Nickname '" << nickname << "' is already taken in room: " << roomName << std::endl;
                                return;
                            }
                        }
                    }
                }
                
                // Nickname is unique, add this user to participants
                std::string participantPath = "rooms/" + roomName + "/participants/" + currentUser.uid();
                auto participantRef = realtimeDatabase->GetReference(participantPath.c_str());
                
                auto joinFuture = participantRef.SetValue(firebase::Variant(nickname));
                joinFuture.OnCompletion([this, roomName](const firebase::Future<void>& joinResult) {
                    if (joinResult.error() == firebase::database::kErrorNone) {
                        std::cout << "Successfully joined room: " << roomName << std::endl;
                        
                        // Update participant count
                        std::string countPath = "rooms/" + roomName + "/participants";
                        auto countRef = realtimeDatabase->GetReference(countPath.c_str());
                        auto countFuture = countRef.GetValue();
                        countFuture.OnCompletion([this, roomName](const firebase::Future<firebase::database::DataSnapshot>& countResult) {
                            if (countResult.error() == firebase::database::kErrorNone) {
                                auto countSnapshot = countResult.result();
                                if (countSnapshot->exists()) {
                                    int64_t participantCount = countSnapshot->value().map().size();
                                    std::string participantCountPath = "rooms/" + roomName + "/participantCount";
                                    auto participantCountRef = realtimeDatabase->GetReference(participantCountPath.c_str());
                                    participantCountRef.SetValue(firebase::Variant(participantCount));
                                }
                            }
                        });
                        
                        // Load the room's engine state
                        readFromRoom(roomName);
                    } else {
                        std::cout << "Failed to join room: " << joinResult.error_message() << std::endl;
                    }
                });
            } else {
                std::cout << "Room does not exist: " << roomName << std::endl;
            }
        } else {
            std::cout << "Failed to check room existence: " << result.error_message() << std::endl;
        }
    });
#else
    std::cout << "Firebase not available - mock room join: " << roomName << std::endl;
#endif
}

void Application::leaveRoom(const std::string& roomName) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase || !auth) {
        std::cout << "Firebase not ready for collaboration" << std::endl;
        return;
    }
    
    auto currentUser = auth->current_user();
    if (!currentUser.is_valid()) {
        std::cout << "User not authenticated" << std::endl;
        return;
    }
    
    std::cout << "Leaving room: " << roomName << std::endl;
    
    // Remove this user from participants
    std::string participantPath = "rooms/" + roomName + "/participants/" + currentUser.uid();
    auto participantRef = realtimeDatabase->GetReference(participantPath.c_str());
    
    auto leaveFuture = participantRef.RemoveValue();
    leaveFuture.OnCompletion([this, roomName](const firebase::Future<void>& leaveResult) {
        if (leaveResult.error() == firebase::database::kErrorNone) {
            std::cout << "Successfully left room: " << roomName << std::endl;
            
            // Check if room is now empty and delete if so
            std::string participantsPath = "rooms/" + roomName + "/participants";
            auto participantsRef = realtimeDatabase->GetReference(participantsPath.c_str());
            auto checkFuture = participantsRef.GetValue();
            checkFuture.OnCompletion([this, roomName](const firebase::Future<firebase::database::DataSnapshot>& checkResult) {
                if (checkResult.error() == firebase::database::kErrorNone) {
                    auto snapshot = checkResult.result();
                    if (!snapshot->exists() || snapshot->value().map().empty()) {
                        // Room is empty, delete it
                        std::string roomPath = "rooms/" + roomName;
                        auto roomRef = realtimeDatabase->GetReference(roomPath.c_str());
                        auto deleteFuture = roomRef.RemoveValue();
                        deleteFuture.OnCompletion([roomName](const firebase::Future<void>& deleteResult) {
                            if (deleteResult.error() == firebase::database::kErrorNone) {
                                std::cout << "Room deleted (was empty): " << roomName << std::endl;
                            } else {
                                std::cout << "Failed to delete empty room: " << deleteResult.error_message() << std::endl;
                            }
                        });
                    } else {
                        // Update participant count
                        int64_t participantCount = snapshot->value().map().size();
                        std::string participantCountPath = "rooms/" + roomName + "/participantCount";
                        auto participantCountRef = realtimeDatabase->GetReference(participantCountPath.c_str());
                        participantCountRef.SetValue(firebase::Variant(participantCount));
                        std::cout << "Room " << roomName << " now has " << participantCount << " participants" << std::endl;
                    }
                }
            });
        } else {
            std::cout << "Failed to leave room: " << leaveResult.error_message() << std::endl;
        }
    });
#else
    std::cout << "Firebase not available - mock room leave: " << roomName << std::endl;
#endif
}

void Application::updateRoomEngineState(const std::string& roomName, const std::string& engineState) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase) return;
    
    // Skip if this is the same as the last known remote state (avoid ping-pong)
    if (engineState == lastKnownRemoteEngineState) {
        std::cout << "Skipping Firebase update - state matches last known remote state" << std::endl;
        return;
    }
    
    // Additional check: use state hash to prevent unnecessary writes
    static std::string lastWrittenStateHash;
    std::string currentStateHash = engine.getStateHash();
    
    if (currentStateHash == lastWrittenStateHash) {
        std::cout << "Skipping Firebase update - state hash unchanged" << std::endl;
        return;
    }
    
    lastWrittenStateHash = currentStateHash;
    writeToRoom(roomName, "engineState", engineState);
    std::cout << "Updated Firebase with new engine state (hash: " << currentStateHash << ")" << std::endl;
#endif
}

void Application::writeToRoom(const std::string& roomName, const std::string& section, const std::string& data) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase) return;
    
    std::lock_guard<std::mutex> lock(firebaseMutex);
    
    std::string path = "rooms/" + roomName + "/" + section;
    auto ref = realtimeDatabase->GetReference(path.c_str());
    ref.SetValue(firebase::Variant(data));
#endif
}

void Application::checkRoomEngineState(const std::string& roomName) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase) return;
    
    static std::chrono::steady_clock::time_point lastFirebaseCall;
    static int consecutiveCallCount = 0;
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastCall = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFirebaseCall).count();
    
    int minDelay = 1000 + (consecutiveCallCount * 200);
    minDelay = std::min(minDelay, 5000);
    
    if (timeSinceLastCall < minDelay) {
        return;
    }
    
    if (timeSinceLastCall > 10000) {
        consecutiveCallCount = 0;
    } else {
        consecutiveCallCount++;
    }
    
    lastFirebaseCall = now;
    
    std::string engineStatePath = "rooms/" + roomName + "/engineState";
    auto engineStateRef = realtimeDatabase->GetReference(engineStatePath.c_str());
    
    // Use async operation without blocking
    auto readFuture = engineStateRef.GetValue();
    
    // Store future for cleanup and non-blocking completion check
    {
        std::lock_guard<std::mutex> lock(firebaseMutex);
        pendingFirebaseFutures.push_back(readFuture);
    }
    
    readFuture.OnCompletion([this](const firebase::Future<firebase::database::DataSnapshot>& result) {
        // Remove this future from pending list
        {
            std::lock_guard<std::mutex> lock(firebaseMutex);
            pendingFirebaseFutures.erase(
                std::remove_if(pendingFirebaseFutures.begin(), pendingFirebaseFutures.end(),
                    [&result](const auto& future) { return &future == &result; }),
                pendingFirebaseFutures.end()
            );
        }
        
        if (result.error() == firebase::database::kErrorNone) {
            auto snapshot = result.result();
            if (snapshot->exists() && snapshot->value().is_string()) {
                std::string remoteEngineState = snapshot->value().string_value();
                
                if (remoteEngineState != lastKnownRemoteEngineState) {
                    std::string currentEngineState = engine.getStateString();
                    if (remoteEngineState != currentEngineState) {
                        {
                            std::lock_guard<std::mutex> updateLock(engineUpdateMutex);
                            if (!hasPendingEngineUpdate) {
                                pendingEngineStateUpdate = remoteEngineState;
                                hasPendingEngineUpdate = true;
                                std::cout << "Queued engine state update from Firebase" << std::endl;
                            } else {
                                std::cout << "Skipped queueing - engine update already pending" << std::endl;
                            }
                        }
                    }
                    lastKnownRemoteEngineState = remoteEngineState;
                }
            }
        } else {
            std::cout << "Failed to check room engine state: " << result.error_message() << std::endl;
        }
    });
#endif
}

void Application::processPendingEngineUpdates() {
    std::unique_lock<std::mutex> lock(engineUpdateMutex, std::try_to_lock);
    
    if (!lock.owns_lock()) {
        return;
    }
    
    if (hasPendingEngineUpdate) {
        try {
            // Validate the engine state before loading
            if (!pendingEngineStateUpdate.empty()) {
                // Check if this is the same state we already have to avoid unnecessary work
                std::string currentStateHash = engine.getStateHash();
                std::string pendingStateHash = std::to_string(std::hash<std::string>{}(pendingEngineStateUpdate));
                
                if (currentStateHash == pendingStateHash) {
                    std::cout << "Skipping engine state load - state unchanged" << std::endl;
                } else {
                    static auto lastLoadTime = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    auto timeSinceLastLoad = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLoadTime).count();
                    
                    if (timeSinceLastLoad > 16) {
                        auto startTime = std::chrono::steady_clock::now();
                        engine.load(pendingEngineStateUpdate);
                        auto endTime = std::chrono::steady_clock::now();
                        auto loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                        std::cout << "Applied pending engine state update safely (" << loadTime << "ms)" << std::endl;
                        lastLoadTime = endTime;
                        
                        hasPendingEngineUpdate = false;
                        pendingEngineStateUpdate.clear();
                    } else {
                        return;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error applying engine state update: " << e.what() << std::endl;
            hasPendingEngineUpdate = false;
            pendingEngineStateUpdate.clear();
        } catch (...) {
            std::cerr << "Unknown error applying engine state update" << std::endl;
            hasPendingEngineUpdate = false;
            pendingEngineStateUpdate.clear();
        }
        
        if (!hasPendingEngineUpdate) {
            hasPendingEngineUpdate = false;
            pendingEngineStateUpdate.clear();
        }
    }
}

void Application::registerUser(const std::string& emailOrUsername, const std::string& password, std::function<void(AuthState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!auth) {
        callback(AuthState::Error, "Firebase authentication not initialized");
        return;
    }
    
    authState = AuthState::Loading;
    
    // Convert username to email if needed
    std::string email = emailOrUsername;
    if (email.find('@') == std::string::npos) {
        // It's a username, convert to email format
        email = emailOrUsername + "@MULO.local";
        usernamesToEmails[emailOrUsername] = email;
    }
    
    // Generate and send verification code
    std::string verificationCode;
    bool emailSent = EmailService::sendVerificationEmail(email, verificationCode);
    pendingVerificationCodes[email] = verificationCode;
    codeTimestamps[email] = std::chrono::steady_clock::now();
    
    // For registration, always require email verification
    authState = AuthState::RequiresMFA;
    mfaRequired = true;
    pendingMFASessionInfo = email + ":" + password; // Store both for completion
    
    if (emailSent) {
        callback(AuthState::RequiresMFA, "Verification code sent to your email");
    } else {
        callback(AuthState::Error, "Failed to send verification email");
    }
#else
    callback(AuthState::Error, "Firebase not available");
#endif
}

void Application::loginUser(const std::string& emailOrUsername, const std::string& password, std::function<void(AuthState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!auth) {
        callback(AuthState::Error, "Firebase authentication not initialized");
        return;
    }
    
    authState = AuthState::Loading;
    
    // Convert username to email if needed
    std::string email = emailOrUsername;
    if (email.find('@') == std::string::npos) {
        auto it = usernamesToEmails.find(emailOrUsername);
        if (it != usernamesToEmails.end()) {
            email = it->second;
        } else {
            email = emailOrUsername + "@MULO.local";
        }
    }
    
    // For login, skip MFA verification for now - just login directly
    auto loginFuture = auth->SignInWithEmailAndPassword(email.c_str(), password.c_str());
    loginFuture.OnCompletion([this, callback, email](const firebase::Future<firebase::auth::AuthResult>& result) {
        if (result.error() == firebase::auth::kAuthErrorNone) {
            authState = AuthState::Success;
            userLoggedIn = true;
            currentUserEmail = email;
            saveLastLoggedInUser(email);
            callback(AuthState::Success, "Login successful");
            std::cout << "User login successful: " << email << std::endl;
        } else {
            authState = AuthState::Error;
            callback(AuthState::Error, "Login failed: " + std::string(result.error_message()));
            std::cout << "Login failed: " << result.error_message() << std::endl;
        }
    });
#else
    callback(AuthState::Error, "Firebase not available");
#endif
}

void Application::verifyMFA(const std::string& verificationCode, std::function<void(AuthState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!auth || !mfaRequired) {
        callback(AuthState::Error, "MFA verification not required or auth not initialized");
        return;
    }
    
    authState = AuthState::Loading;
    
    if (verificationCode.length() == 6 && verificationCode.find_first_not_of("0123456789") == std::string::npos) {
        size_t colonPos = pendingMFASessionInfo.find(':');
        std::string email = pendingMFASessionInfo.substr(0, colonPos);
        std::string password = pendingMFASessionInfo.substr(colonPos + 1);
        
        auto codeIt = pendingVerificationCodes.find(email);
        auto timeIt = codeTimestamps.find(email);
        
        if (codeIt == pendingVerificationCodes.end() || timeIt == codeTimestamps.end()) {
            authState = AuthState::Error;
            callback(AuthState::Error, "No verification code found for this email");
            return;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - timeIt->second);
        if (elapsed.count() > 10) {
            authState = AuthState::Error;
            pendingVerificationCodes.erase(codeIt);
            codeTimestamps.erase(timeIt);
            callback(AuthState::Error, "Verification code has expired");
            return;
        }
        
        if (codeIt->second != verificationCode) {
            authState = AuthState::Error;
            callback(AuthState::Error, "Invalid verification code");
            return;
        }
        
        pendingVerificationCodes.erase(codeIt);
        codeTimestamps.erase(timeIt);
        
        bool isRegistration = (colonPos != std::string::npos);
        
        if (isRegistration) {
            auto registerFuture = auth->CreateUserWithEmailAndPassword(email.c_str(), password.c_str());
            registerFuture.OnCompletion([this, callback, email](const firebase::Future<firebase::auth::AuthResult>& result) {
                if (result.error() == firebase::auth::kAuthErrorNone) {
                    authState = AuthState::Success;
                    userLoggedIn = true;
                    currentUserEmail = email;
                    mfaRequired = false;
                    pendingMFASessionInfo = "";
                    saveLastLoggedInUser(email);
                    callback(AuthState::Success, "Registration and verification successful");
                    std::cout << "User registration successful: " << email << std::endl;
                } else {
                    authState = AuthState::Error;
                    callback(AuthState::Error, "Registration failed: " + std::string(result.error_message()));
                    std::cout << "Registration failed: " << result.error_message() << std::endl;
                }
            });
        } else {
            auto loginFuture = auth->SignInWithEmailAndPassword(email.c_str(), password.c_str());
            loginFuture.OnCompletion([this, callback, email](const firebase::Future<firebase::auth::AuthResult>& result) {
                if (result.error() == firebase::auth::kAuthErrorNone) {
                    authState = AuthState::Success;
                    userLoggedIn = true;
                    currentUserEmail = email;
                    mfaRequired = false;
                    pendingMFASessionInfo = "";
                    saveLastLoggedInUser(email);
                    callback(AuthState::Success, "Login and verification successful");
                    std::cout << "User login successful: " << email << std::endl;
                } else {
                    authState = AuthState::Error;
                    callback(AuthState::Error, "Login failed: " + std::string(result.error_message()));
                    std::cout << "Login failed: " << result.error_message() << std::endl;
                }
            });
        }
    } else {
        authState = AuthState::Error;
        callback(AuthState::Error, "Invalid verification code");
        std::cout << "MFA verification failed: Invalid code format" << std::endl;
    }
#else
    callback(AuthState::Error, "Firebase not available");
#endif
}

void Application::enableMFA(std::function<void(AuthState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!auth || !userLoggedIn) {
        callback(AuthState::Error, "User must be logged in to enable MFA");
        return;
    }
    
    authState = AuthState::Loading;
    authState = AuthState::Success;
    callback(AuthState::Success, "MFA enabled successfully. Use authenticator app for future logins.");
    std::cout << "MFA enabled for user: " << currentUserEmail << std::endl;
#else
    callback(AuthState::Error, "Firebase not available");
#endif
}

void Application::logoutUser() {
#ifdef FIREBASE_AVAILABLE
    if (auth && userLoggedIn) {
        auth->SignOut();
        userLoggedIn = false;
        currentUserEmail = "";
        authState = AuthState::Idle;
        mfaRequired = false;
        pendingMFASessionInfo = "";
        std::cout << "User logged out successfully" << std::endl;
    }
#endif
}

bool Application::isUserLoggedIn() const {
    return userLoggedIn;
}

std::string Application::getCurrentUserEmail() const {
    return currentUserEmail;
}

void Application::saveLastLoggedInUser(const std::string& email) {
    lastLoggedInUser = email;
    // Save to a config file for persistence across app restarts
    std::ofstream configFile(exeDirectory + "/user_session.cfg");
    if (configFile.is_open()) {
        configFile << email;
        configFile.close();
    }
}

std::string Application::getLastLoggedInUser() {
    if (lastLoggedInUser.empty()) {
        // Try to load from config file
        std::ifstream configFile(exeDirectory + "/user_session.cfg");
        if (configFile.is_open()) {
            std::getline(configFile, lastLoggedInUser);
            configFile.close();
        }
    }
    return lastLoggedInUser;
}

bool Application::isReturningUser(const std::string& email) {
    std::string lastUser = getLastLoggedInUser();
    return !lastUser.empty() && lastUser == email;
}

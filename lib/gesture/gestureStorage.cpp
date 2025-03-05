#include "gestureStorage.h"
#include "featureNormalyze.h"
#include "Logger.h"
GestureStorage gestureStorage;

GestureStorage::GestureStorage()
{
    if (!initializeFileSystem())
    {
        Logger::getInstance().log(String("Failed to initialize gesture storage system"));
    }
}

bool GestureStorage::initializeFileSystem()
{
    if (!LittleFS.begin(true))
    {
        Logger::getInstance().log("Failed to mount LittleFS");
        return false;
    }
    return ensureFileExists();
}

bool GestureStorage::ensureFileExists()
{
    if (!LittleFS.exists(GESTURE_STORAGE_FILE))
    {
        File file = LittleFS.open(GESTURE_STORAGE_FILE, "w");
        if (!file)
        {
            Logger::getInstance().log("Failed to create gesture storage file");
            return false;
        }
        file.print("{}"); // Initialize with empty JSON object
        file.close();
    }
    return true;
}

bool GestureStorage::saveGestureFeature(uint8_t gestureID, const float *features, size_t featureCount)
{
    if (gestureID >= MAX_GESTURES)
    {
        Logger::getInstance().log("Error: Invalid gesture ID (must be 0-8)");
        return false;
    }

    if (!initializeFileSystem())
    {
        return false;
    }

    // Load existing data
    File file = LittleFS.open(GESTURE_STORAGE_FILE, "r");
    if (!file)
    {
        Logger::getInstance().log("Failed to open gesture storage file for reading");
        return false;
    }

    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        Logger::getInstance().log("Failed to parse gesture storage file");
        return false;
    }

    // Get or create gesture object
    if (!doc.containsKey(String(gestureID)))
    {
        doc.createNestedArray(String(gestureID));
    }

    JsonArray gestureArray = doc[String(gestureID)];

    // Create new feature array
    JsonArray newSample = gestureArray.createNestedArray();
    for (size_t i = 0; i < featureCount; i++)
    {
        if (!newSample.add(features[i]))
        {
            Logger::getInstance().log("Failed to add feature to array");
            return false;
        }
    }

    // Debug print current array size
    Logger::getInstance().log("GestureID " + String(gestureID) + " now has " + String(gestureArray.size()) + " samples");

    // Remove oldest sample if limit exceeded
    while (gestureArray.size() > MAX_SAMPLES)
    {
        Logger::getInstance().log("Removing oldest sample from gesture " + String(gestureID));

        gestureArray.remove(0);
    }

    // Save updated data
    file = LittleFS.open(GESTURE_STORAGE_FILE, "w");
    if (!file)
    {
        Logger::getInstance().log("Failed to open gesture storage file for writing");
        return false;
    }

    if (serializeJson(doc, file) == 0)
    {
        Logger::getInstance().log("Failed to write gesture data");
        file.close();
        return false;
    }

    file.close();
    return true;
}

float ***GestureStorage::loadMatrixFromBinary(const char *filename, size_t &numGestures, size_t &numSamples, size_t &numFeatures)
{
    if (!initializeFileSystem())
    {
        Logger::getInstance().log("Failed to initialize filesystem for binary read");
        return nullptr;
    }

    if (!LittleFS.exists(filename))
    {

        Logger::getInstance().log("File not found: " + String(filename) +
                                  "\nAttempting to create binary file from JSON data...");
        // Convert JSON to matrix
        float ***matrix = convertJsonToMatrix3D(numGestures, numSamples, numFeatures);
        if (!matrix)
        {
            Logger::getInstance().log("Failed to convert JSON to matrix");
            return nullptr;
        }

        // Save converted matrix to binary
        if (!saveMatrixToBinary(filename, matrix, numGestures, numSamples, numFeatures))
        {
            Logger::getInstance().log("Failed to save converted matrix to binary file");
            clearMatrix3D(matrix, numGestures, numSamples);
            return nullptr;
        }

        Logger::getInstance().log("Successfully created binary file from JSON data");
    }

    File file = LittleFS.open(filename, "r");
    if (!file)
    {
        Logger::getInstance().log("Failed to open binary file: " + String(filename));
        return nullptr;
    }

    // Read dimensions with improved error handling
    size_t dims[3];
    if (file.read((uint8_t *)&dims, sizeof(dims)) != sizeof(dims))
    {
        Logger::getInstance().log("Failed to read matrix dimensions");
        file.close();
        return nullptr;
    }

    numGestures = dims[0];
    numSamples = dims[1];
    numFeatures = dims[2];

    Logger::getInstance().log("Loading binary matrix  - Gesture:" + String(numGestures) + " Samples:" + String(numSamples) + " Feature:" + String(numFeatures));

    // Validate dimensions before allocation
    if (numGestures == 0 || numSamples == 0 || numFeatures == 0)
    {
        Logger::getInstance().log("Invalid matrix dimensions in file");
        file.close();
        return nullptr;
    }

    // Allocate memory for 3D matrix with rollback on failure
    float ***matrix = nullptr;
    try
    {
        matrix = new float **[numGestures];
        for (size_t g = 0; g < numGestures; g++)
        {
            matrix[g] = new float *[numSamples];
            for (size_t s = 0; s < numSamples; s++)
            {
                matrix[g][s] = new float[numFeatures](); // Initialize with zeros
            }
        }
    }
    catch (const std::bad_alloc &e)
    {
        Logger::getInstance().log("Memory allocation failed: " + String(e.what()));
        clearMatrix3D(matrix, numGestures, numSamples);
        file.close();
        return nullptr;
    }

    // Read matrix data with progress tracking
    size_t totalSamples = numGestures * numSamples;
    size_t samplesRead = 0;

    for (size_t g = 0; g < numGestures; g++)
    {
        for (size_t s = 0; s < numSamples; s++)
        {
            if (file.read((uint8_t *)matrix[g][s], sizeof(float) * numFeatures) != sizeof(float) * numFeatures)
            {
                Logger::getInstance().log("Read failed at gesture " + String(g) + " sample " + String(s));
                file.close();
                clearMatrix3D(matrix, numGestures, numSamples);
                return nullptr;
            }
            samplesRead++;

          //  // Progress update every 10%
          //  if (totalSamples > 0 && (samplesRead * 100 / totalSamples) % 25 == 0)
          //  {
          //      Logger::getInstance().log("Loaded: " + String((samplesRead * 100 / totalSamples)) + "%");
          //  }
        }
    }

    file.close();

    Logger::getInstance().log("Successfully loaded " + String(samplesRead) + " samples from binary file");

    return matrix;
}

bool GestureStorage::loadGestureFeatures()
{
    if (!initializeFileSystem())
    {
        return false;
    }

    File file = LittleFS.open(GESTURE_STORAGE_FILE, "r");
    if (!file)
    {
        Logger::getInstance().log("Failed to open gesture storage file");
        return false;
    }

    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        Logger::getInstance().log("Failed to parse gesture storage file");
        return false;
    }

    return true;
}

bool GestureStorage::clearGestureFeatures(uint8_t gestureID)
{
    if (!initializeFileSystem())
    {
        return false;
    }

    // If no specific gesture ID provided, clear everything
    if (gestureID == UINT8_MAX)
    {
        File file = LittleFS.open(GESTURE_STORAGE_FILE, "w");
        if (!file)
        {
            Logger::getInstance().log("Failed to open gesture storage file for clearing");
            return false;
        }

        file.print("{}"); // Reset to empty JSON
        file.close();
        doc.clear();
        Logger::getInstance().log("Cleared all gesture features");
        return true;
    }

    // Validate gesture ID
    if (gestureID >= MAX_GESTURES)
    {
        Logger::getInstance().log("Invalid gestureID:" + String(gestureID));
        return false;
    }

    // Load existing data
    File file = LittleFS.open(GESTURE_STORAGE_FILE, "r");
    if (!file)
    {
        Logger::getInstance().log("Failed to open gesture storage file for reading");
        return false;
    }

    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        Logger::getInstance().log("Failed to parse gesture storage file");
        return false;
    }

    // Remove specific gesture if it exists
    String gestureKey = String(gestureID);
    if (doc.containsKey(gestureKey))
    {
        doc.remove(gestureKey);
        Logger::getInstance().log("Removed gestureID: " + String(gestureID));

        // Save updated data
        file = LittleFS.open(GESTURE_STORAGE_FILE, "w");
        if (!file)
        {
            Logger::getInstance().log("Failed to open gesture storage file for writing");
            return false;
        }

        if (serializeJson(doc, file) == 0)
        {
            Logger::getInstance().log("Failed to write updated gesture data");
            file.close();
            return false;
        }

        file.close();
        return true;
    }

    Logger::getInstance().log("Gesture " + String(gestureID) + " not found - nothing to remove");
    return false;
}
void GestureStorage::printJsonFeatures()
{
    if (!loadGestureFeatures())
    {
        return;
    }

    String logMessage = "Stored gesture features:\n";

    JsonObject obj = doc.as<JsonObject>();
    for (JsonPair kv : obj)
    {
        logMessage += "Gesture " + String(kv.key().c_str()) + ":\n";
        JsonArray samples = kv.value().as<JsonArray>();
        for (JsonVariant sample : samples)
        {
            logMessage += "  [ ";
            JsonArray features = sample.as<JsonArray>();
            for (JsonVariant feature : features)
            {
                logMessage += String(feature.as<float>()) + " ";
            }
            logMessage += "]\n";
        }
    }

    Logger::getInstance().log(logMessage);
}


float ***GestureStorage::convertJsonToMatrix3D(size_t &numGestures, size_t &numSamples, size_t &numFeatures)
{
    if (!loadGestureFeatures() || doc.isNull())
    {
        Logger::getInstance().log("Failed to load or parse JSON features");
        return nullptr;
    }

    // Use fixed matrix dimensions based on MAX_GESTURES
    numGestures = MAX_GESTURES;
    numSamples = SIZE_MAX;
    numFeatures = 0;

    // First pass: validate structure and calculate samples/features
    for (JsonPair kv : doc.as<JsonObject>())
    {
        if (!kv.value().is<JsonArray>())
        {
            Logger::getInstance().log(String("Invalid data type for gesture ") + String(kv.key().c_str()), false);
            return nullptr;
        }

        // Get actual gesture ID from JSON key
        uint8_t gestureID = atoi(kv.key().c_str());
        if (gestureID >= MAX_GESTURES)
        {
            Logger::getInstance().log(String("Skipping invalid gesture ID: ") + String(gestureID), false);
            continue;
        }

        JsonArray samples = kv.value().as<JsonArray>();
        numSamples = min(numSamples, samples.size());

        for (JsonVariant sample : samples)
        {
            if (!sample.is<JsonArray>())
            {
                Logger::getInstance().log(String("Invalid sample type in gesture ") + String(kv.key().c_str()), false);
                return nullptr;
            }

            JsonArray features = sample.as<JsonArray>();
            if (numFeatures == 0)
            {
                numFeatures = features.size();
            }
            else if (features.size() != numFeatures)
            {

                Logger::getInstance().log(String("Feature count mismatch in gesture") + String(kv.key().c_str()), false);


                return nullptr;
            }
        }
    }

    if (numSamples == 0 || numGestures == 0 || numFeatures == 0)
    {
        Logger::getInstance().log("Error: Invalid dimensions - G:" + String(numGestures) + " S:" + String(numSamples) + " F:" + String(numFeatures));
        return nullptr;
    }

    Logger::getInstance().log("Allocating 3D matrix - Gestures:" + String(numGestures) + " Samples:" + String(numSamples) + " Features:" + String(numFeatures));

    // Allocate memory with exception handling
    float ***matrix = nullptr;
    try
    {
        matrix = new float **[numGestures];
        for (size_t i = 0; i < numGestures; i++)
        {
            matrix[i] = new float *[numSamples];
            for (size_t j = 0; j < numSamples; j++)
            {
                matrix[i][j] = new float[numFeatures](); // Initialize with zeros
            }
        }
    }
    catch (const std::bad_alloc &e)
    {
        Logger::getInstance().log("Memory allocation failed: " + String(e.what()));
        clearMatrix3D(matrix, numGestures, numSamples);
        return nullptr;
    }

    // Second pass: populate matrix using actual gesture IDs
    for (JsonPair kv : doc.as<JsonObject>())
    {
        uint8_t gestureID = atoi(kv.key().c_str());
        if (gestureID >= MAX_GESTURES)
            continue;

        JsonArray samples = kv.value().as<JsonArray>();

        for (size_t i = 0; i < numSamples; i++)
        {
            JsonArray featureArray = samples[i];

            for (size_t j = 0; j < numFeatures; j++)
            {
                if (!featureArray[j].is<float>())
                {
                    Logger::getInstance().log("Invalid feature type at gesture " + String(gestureID) + " sample " + String(i) + " feature " + String(j));
                    clearMatrix3D(matrix, numGestures, numSamples);
                    return nullptr;
                }
                matrix[gestureID][i][j] = featureArray[j].as<float>();
            }
        }

       // Logger::getInstance().log("Loaded gesture " + String(gestureID));
    }
    return matrix;
}

bool GestureStorage::saveMatrixToBinary(const char *filename, float ***matrix, size_t numGestures, size_t numSamples, size_t numFeatures)
{
    if (!matrix || numGestures == 0 || numSamples == 0 || numFeatures == 0)
    {
        Logger::getInstance().log("Error: Invalid matrix parameters");
        return false;
    }

    // Force remove existing file to ensure clean write
    if (LittleFS.exists(filename))
    {
        if (!LittleFS.remove(filename))
        {
            Logger::getInstance().log("Failed to remove existing file: " + String(filename));
            return false;
        }
    }

    File file = LittleFS.open(filename, "w");
    if (!file)
    {
        Logger::getInstance().log("Failed to create file: " + String(filename));
        return false;
    }

    // Write dimensions with error checking
    size_t bytesWritten = 0;
    bytesWritten += file.write((uint8_t *)&numGestures, sizeof(size_t));
    bytesWritten += file.write((uint8_t *)&numSamples, sizeof(size_t));
    bytesWritten += file.write((uint8_t *)&numFeatures, sizeof(size_t));

    if (bytesWritten != 3 * sizeof(size_t))
    {
        Logger::getInstance().log("Failed to write matrix dimensions");
        file.close();
        return false;
    }

    // Write matrix data with progress tracking
    size_t totalSamples = numGestures * numSamples;
    size_t samplesWritten = 0;

    for (size_t g = 0; g < numGestures; g++)
    {
        for (size_t s = 0; s < numSamples; s++)
        {
            size_t wrote = file.write((uint8_t *)matrix[g][s], sizeof(float) * numFeatures);
            if (wrote != sizeof(float) * numFeatures)
            {
                Logger::getInstance().log("Write failed at gesture " + String(g) + " sample " + String(s));
                file.close();
                return false;
            }
            samplesWritten++;

            // Progress update every 10%
            if (totalSamples > 0 && (samplesWritten * 100 / totalSamples) % 10 == 0)
            {
                Logger::getInstance().log(String("Progress: ")+String(samplesWritten * 100 / totalSamples)+String("%"));
            }
        }
    }

    file.close(); // Close file BEFORE cleaning memory

    // Only clear matrix after successful write
    clearMatrix3D(matrix, numGestures, numSamples);

    Logger::getInstance().log(String("Successfully wrote ")+String(samplesWritten)+String(" samples to binary file"));

    return true;
}
void GestureStorage::clearMatrix3D(float ***&matrix, size_t numGestures, size_t numSamples)
{
    if (!matrix)
        return;

    for (size_t g = 0; g < numGestures; g++)
    {
        if (matrix[g])
        {
            for (size_t s = 0; s < numSamples; s++)
            {
                if (matrix[g][s])
                {
                    delete[] matrix[g][s];
                    matrix[g][s] = nullptr;
                }
            }
            delete[] matrix[g];
            matrix[g] = nullptr;
        }
    }
    delete[] matrix;
    matrix = nullptr;
}

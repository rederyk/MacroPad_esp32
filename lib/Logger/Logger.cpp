/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "Logger.h"


Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::log(const String &message, bool newLine)
{
    // Inserisce il messaggio nel buffer circolare (con protezione in caso di interruzioni)
    noInterrupts();
    if (bufferCount < BUFFER_SIZE)
    {
        logBuffer[bufferWriteIndex] = { message, newLine };
        bufferWriteIndex = (bufferWriteIndex + 1) % BUFFER_SIZE;
        bufferCount++;
    }
    else
    {
        // Buffer pieno: sovrascrive l'elemento più vecchio
        logBuffer[bufferWriteIndex] = { message, newLine };
        bufferWriteIndex = (bufferWriteIndex + 1) % BUFFER_SIZE;
        bufferReadIndex = (bufferReadIndex + 1) % BUFFER_SIZE;
    }
    interrupts();


}

void Logger::processBuffer()
{
    while (bufferCount > 0)
    {
        LogEntry entry = logBuffer[bufferReadIndex];

        // Invio sull'output seriale, se abilitato
        if (serialEnabled && Serial)
        {
            if (entry.newLine)
            {
                Serial.println(entry.message);
            }
            else
            {
                Serial.print(entry.message);
            }
        }

        // Invio agli output del web server, se attivo
        if (webServerActive)
        {
            for (auto &output : outputs)
            {
                output(entry.message);
            }
        }

        bufferReadIndex = (bufferReadIndex + 1) % BUFFER_SIZE;
        bufferCount--;
    }
}

void Logger::addOutput(std::function<void(const String &)> output)
{
    outputs.push_back(output);
}

void Logger::setWebServerActive(bool active)
{
    webServerActive = active;
}

void Logger::setSerialEnabled(bool enabled)
{
    serialEnabled = enabled;
}

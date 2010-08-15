/*=====================================================================

PIXHAWK Micro Air Vehicle Flying Robotics Toolkit

(c) 2009, 2010 PIXHAWK PROJECT  <http://pixhawk.ethz.ch>

This file is part of the PIXHAWK project

    PIXHAWK is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PIXHAWK is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PIXHAWK. If not, see <http://www.gnu.org/licenses/>.

======================================================================*/

/**
 * @file
 *   @brief Cross-platform support for serial ports
 *
 *   @author Lorenz Meier <mavteam@student.ethz.ch>
 *
 */

#include <QTimer>
#include <QDebug>
#include <QMutexLocker>
#include "SerialLink.h"
#include <MG.h>
#ifdef _WIN32
#include "windows.h"
#endif


SerialLink::SerialLink(QString portname, BaudRateType baudrate, FlowType flow, ParityType parity, DataBitsType dataBits, StopBitsType stopBits) :
	porthandle( portname.trimmed() ),
	baudrate(baudrate),
	flow(flow),
	parity(parity),
	dataBits(dataBits),
	stopBits(stopBits),
	timeout(1) ///< The timeout controls how long the program flow should wait for new serial bytes. As we're polling, we don't want to wait at all.

{
#ifdef _WIN32
    // Port names above 20 need the network path format - if the port name is not already in this format
    // catch this special case
    if (SerialLink::porthandle.size() > 0 && !SerialLink::porthandle.startsWith("\\"))
    {
        // Append \\.\ before the port handle. Additional backslashes are used for escaping.
        SerialLink::porthandle = "\\\\.\\" + SerialLink::porthandle;
    }
#endif
    // Set unique ID and add link to the list of links

    // Set the port name
    if (porthandle == "")
    {
        name = tr("Serial Link");
    }
    else
    {
        name = porthandle;
    }

#ifdef _WIN3232
    // Windows 32bit & 64bit serial connection
    winPort = CreateFile(porthandle,
                         GENERIC_READ | GENERIC_WRITE,
                         0,
                         0,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         0);
    if(winPort==INVALID_HANDLE_VALUE){
        if(GetLastError()==ERROR_FILE_NOT_FOUND){
            //serial port does not exist. Inform user.
        }
        //some other error occurred. Inform user.
    }
#else
    // *nix (Linux, MacOS tested) serial port support
    port = new QextSerialPort(porthandle, QextSerialPort::Polling);
    port->setTimeout(timeout); // Timeout of 0 ms, we don't want to wait for data, we just poll again next time
    port->setBaudRate(baudrate);
    port->setFlowControl(flow);
    port->setParity(parity);
    port->setDataBits(dataBits);
    port->setStopBits(stopBits);
#endif
}

SerialLink::~SerialLink()
{
    close();
    if(port) delete port;
}


/**
 * @brief Runs the thread
 *
 **/
void SerialLink::run() {

    // Initialize the connection
    hardwareConnect();

    // Qt way to make clear what a while(1) loop does
    forever {
        // Check if new bytes have arrived, if yes, emit the notification signal
        checkForBytes();
        /* Serial data isn't arriving that fast normally, this saves the thread
                 * from consuming too much processing time
                 */
        MG::SLEEP::msleep(SerialLink::poll_interval);
    }

}


void SerialLink::checkForBytes() {
    /* Check if bytes are available */
    if(port->isOpen()) {
        dataMutex.lock();
        qint64 available = port->bytesAvailable();
        dataMutex.unlock();

        if(available > 0) {
            emit bytesReady(this);
            //qDebug() << "Bytes available" << available << "connected:" << port->isOpen();
        }
    } else {
        emit closed();
    }

}


qint64 SerialLink::write(const char* data, qint64 size) {
	qint64 writtenBytes = 0;

	if ( port->isOpen() )
	{
		writtenBytes = port->write(data, size);
		qDebug() << "Transmitted " << writtenBytes << "bytes:";

		// Increase write counter
		bitsSentTotal += writtenBytes * 8;

		for (qint64 i=0; i<writtenBytes; i++)
		{
			fprintf(stderr,"%02x ", data[i]);
		}
	}

	return writtenBytes;
}

/**
 * @brief Read a number of bytes from the interface.
 *
 * @param data Pointer to the data byte array to write the bytes to
 * @param maxLength The maximum number of bytes to write
 **/
qint64 SerialLink::read(char* data, qint64 maxLength) {
	qint64 receivedBytes = 0;

	if ( port->isOpen() )
	{
		dataMutex.lock();

		qint64 numBytes = port->bytesAvailable();
		if(numBytes > 0)
		{
			// Read as much data in buffer as possible without overflow
			if(maxLength < numBytes) numBytes = maxLength;

			receivedBytes = port->read(data, numBytes);

			// FIXME TODO Check if this method should be preferred over manual read by process
			QByteArray b(data, receivedBytes);
			emit bytesReceived(this, b);

            //qDebug() << "SerialLink::readBytes()" << std::hex << data;
            //            int i;
            //            for (i=0; i<numBytes; i++){
            //                unsigned int v=data[i];
            //
            //                fprintf(stderr,"%02x ", v);
            //            }
            //            fprintf(stderr,"\n");
			bitsReceivedTotal += receivedBytes * 8;
		}

		dataMutex.unlock();
	}
    
	return receivedBytes;
}


/**
 * @brief Get the number of bytes to read.
 *
 * @return The number of bytes to read
 **/
qint64 SerialLink::bytesAvailable() const {
    return port->bytesAvailable();
}

/**
 * @brief Convenience method to emit the bytesReady signal
 **/
void SerialLink::emitBytesReady() {
    emit bytesReady(this);
}

/**
 * @brief Disconnect the connection.
 *
 * @return True if connection has been disconnected, false if connection couldn't be disconnected.
 **/
bool SerialLink::close() {
    //#if !defined _WIN32 || !defined _WIN64
    /* Block the thread until it returns from run() */
    //#endif
    dataMutex.lock();
    port->flush();
    port->close();
    dataMutex.unlock();

    emit closed();
    emit opened(false);

    return !port->isOpen();
}

/**
 * @brief Connect the connection.
 *
 * @return True if connection has been established, false if connection couldn't be established.
 **/
bool SerialLink::open()
{
    qDebug() << "CONNECTING LINK: " << __FILE__ << __LINE__ << "with settings" << porthandle << baudrate << dataBits << parity << stopBits;
    if (!this->isRunning())
    {
        this->start(LowPriority);
    }
    else
    {
        if(isConnected())
        {
            close();
        }
        hardwareConnect();
    }

    return port->isOpen();
}

/**
 * @brief This function is called indirectly by the connect() call.
 *
 * The connect() function starts the thread and indirectly calls this method.
 *
 * @return True if the connection could be established, false otherwise
 * @see connect() For the right function to establish the connection.
 **/
bool SerialLink::hardwareConnect() {

    qDebug() << "Opening serial port" << porthandle;

    QObject::connect(port, SIGNAL(aboutToClose()), this, SIGNAL(closed()));

    port->open(QIODevice::ReadWrite);
    port->setBaudRate(this->baudrate);
    port->setParity(this->parity);
    port->setStopBits(this->stopBits);
    port->setDataBits(this->dataBits);

    statisticsMutex.lock();
    connectionStartTime = MG::TIME::getGroundTimeNow();
    statisticsMutex.unlock();

    bool connectionUp = isConnected();
    if(connectionUp) {
        emit opened();
        emit opened(true);
    }

    return connectionUp;
}
/**
 * @brief Check if connection is active.
 *
 * @return True if link is connected, false otherwise.
 **/
bool SerialLink::isConnected() const {
    return port->isOpen();
}

qint64 SerialLink::getNominalDataRate() const {
    qint64 dataRate = 0;
    switch (baudrate) {
    case BAUD50:
        dataRate = 50;
        break;
    case BAUD75:
        dataRate = 75;
        break;
    case BAUD110:
        dataRate = 110;
        break;
    case BAUD134:
        dataRate = 134;
        break;
    case BAUD150:
        dataRate = 150;
        break;
    case BAUD200:
        dataRate = 200;
        break;
    case BAUD300:
        dataRate = 300;
        break;
    case BAUD600:
        dataRate = 600;
        break;
    case BAUD1200:
        dataRate = 1200;
        break;
    case BAUD1800:
        dataRate = 1800;
        break;
    case BAUD2400:
        dataRate = 2400;
        break;
    case BAUD4800:
        dataRate = 4800;
        break;
    case BAUD9600:
        dataRate = 9600;
        break;
    case BAUD14400:
        dataRate = 14400;
        break;
    case BAUD19200:
        dataRate = 19200;
        break;
    case BAUD38400:
        dataRate = 38400;
        break;
    case BAUD56000:
        dataRate = 56000;
        break;
    case BAUD57600:
        dataRate = 57600;
        break;
    case BAUD76800:
        dataRate = 76800;
        break;
    case BAUD115200:
        dataRate = 115200;
        break;
    case BAUD128000:
        dataRate = 128000;
        break;
    case BAUD256000:
        dataRate = 256000;
        break;
    }
    return dataRate;
}

qint64 SerialLink::getTotalUpstream() const {
    statisticsMutex.lock();
    return bitsSentTotal / ((MG::TIME::getGroundTimeNow() - connectionStartTime) / 1000);
    statisticsMutex.unlock();
}

qint64 SerialLink::getCurrentUpstream() const {
    return 0; // TODO
}

qint64 SerialLink::getMaxUpstream() const {
    return 0; // TODO
}

qint64 SerialLink::getBitsSent() const {
    return bitsSentTotal;
}

qint64 SerialLink::getBitsReceived() const {
    return bitsReceivedTotal;
}

qint64 SerialLink::getTotalDownstream() const {
    statisticsMutex.lock();
    return bitsReceivedTotal / ((MG::TIME::getGroundTimeNow() - connectionStartTime) / 1000);
    statisticsMutex.unlock();
}

qint64 SerialLink::getCurrentDownstream() const {
    return 0; // TODO
}

qint64 SerialLink::getMaxDownstream() const {
    return 0; // TODO
}

bool SerialLink::isFullDuplex() const {
    /* Serial connections are always half duplex */
    return false;
}

int SerialLink::getLinkQuality() const {
    /* This feature is not supported with this interface */
    return -1;
}

const QString& SerialLink::getPortName() const {
    return porthandle;
}

int SerialLink::getBaudRate() const {
    return getNominalDataRate();
}

int SerialLink::getBaudRateType() const {
    return baudrate;
}

int SerialLink::getFlowType() const {
    return flow;
}

int SerialLink::getParityType() const {
    return parity;
}

int SerialLink::getDataBitsType() const {
    return dataBits;
}

int SerialLink::getStopBitsType() const {
    return stopBits;
}

bool SerialLink::setPortName(const QString& portName)
{
    if(portName.trimmed().length() > 0)
    {
        bool reconnect = false;
        if(isConnected()) {
            disconnect();
            reconnect = true;
        }
        this->porthandle = portName.trimmed();
        setName(tr("serial port ") + portName.trimmed());
#ifdef _WIN32
        // Port names above 20 need the network path format - if the port name is not already in this format
        // catch this special case
        if (!this->porthandle.startsWith("\\"))
        {
            // Append \\.\ before the port handle. Additional backslashes are used for escaping.
            this->porthandle = "\\\\.\\" + this->porthandle;
        }
#endif
        delete port;
        port = new QextSerialPort(porthandle, QextSerialPort::Polling);

        port->setBaudRate(baudrate);
        port->setFlowControl(flow);
        port->setParity(parity);
        port->setDataBits(dataBits);
        port->setStopBits(stopBits);
        port->setTimeout(timeout);
        if(reconnect) open();
        return true;
    }
    else
    {
        return false;
    }
}


bool SerialLink::setBaudRateType(int rateIndex)
{
    bool reconnect = false;
    bool accepted = true; // This is changed if none of the data rates matches
    if(isConnected()) {
        disconnect();
        reconnect = true;
    }
    switch (rateIndex) {
    case 0:
        baudrate = BAUD50;
        break;
    case 1:
        baudrate = BAUD75;
        break;
    case 2:
        baudrate = BAUD110;
        break;
    case 3:
        baudrate = BAUD134;
        break;
    case 4:
        baudrate = BAUD150;
        break;
    case 5:
        baudrate = BAUD200;
        break;
    case 6:
        baudrate = BAUD300;
        break;
    case 7:
        baudrate = BAUD600;
        break;
    case 8:
        baudrate = BAUD1200;
        break;
    case 9:
        baudrate = BAUD1800;
        break;
    case 10:
        baudrate = BAUD2400;
        break;
    case 11:
        baudrate = BAUD4800;
        break;
    case 12:
        baudrate = BAUD9600;
        break;
    case 13:
        baudrate = BAUD14400;
        break;
    case 14:
        baudrate = BAUD19200;
        break;
    case 15:
        baudrate = BAUD38400;
        break;
    case 16:
        baudrate = BAUD56000;
        break;
    case 17:
        baudrate = BAUD57600;
        break;
    case 18:
        baudrate = BAUD76800;
        break;
    case 19:
        baudrate = BAUD115200;
        break;
    case 20:
        baudrate = BAUD128000;
        break;
    case 21:
        baudrate = BAUD256000;
        break;
    default:
        // If none of the above cases matches, there must be an error
        accepted = false;
        break;
    }

    dataMutex.lock();
    port->setBaudRate(this->baudrate);
    dataMutex.unlock();
    if(reconnect) open();
    return accepted;
}



bool SerialLink::setBaudRate(int rate)
{
    bool reconnect = false;
    bool accepted = true; // This is changed if none of the data rates matches
    if(isConnected()) {
        close();
        reconnect = true;
    }

    switch (rate) {
    case 50:
        baudrate = BAUD50;
        break;
    case 75:
        baudrate = BAUD75;
        break;
    case 110:
        baudrate = BAUD110;
        break;
    case 134:
        baudrate = BAUD134;
        break;
    case 150:
        baudrate = BAUD150;
        break;
    case 200:
        baudrate = BAUD200;
        break;
    case 300:
        baudrate = BAUD300;
        break;
    case 600:
        baudrate = BAUD600;
        break;
    case 1200:
        baudrate = BAUD1200;
        break;
    case 1800:
        baudrate = BAUD1800;
        break;
    case 2400:
        baudrate = BAUD2400;
        break;
    case 4800:
        baudrate = BAUD4800;
        break;
    case 9600:
        baudrate = BAUD9600;
        break;
    case 14400:
        baudrate = BAUD14400;
        break;
    case 19200:
        baudrate = BAUD19200;
        break;
    case 38400:
        baudrate = BAUD38400;
        break;
    case 56000:
        baudrate = BAUD56000;
        break;
    case 57600:
        baudrate = BAUD57600;
        break;
    case 76800:
        baudrate = BAUD76800;
        break;
    case 115200:
        baudrate = BAUD115200;
        break;
    case 128000:
        baudrate = BAUD128000;
        break;
    case 256000:
        baudrate = BAUD256000;
        break;
    default:
        // If none of the above cases matches, there must be an error
        accepted = false;
        break;
    }

    port->setBaudRate(this->baudrate);
    if(reconnect) open();
    return accepted;
}

bool SerialLink::setFlowType(int flow) {
    bool reconnect = false;
    bool accepted = true;
    if(isConnected()) {
        close();
        reconnect = true;
    }

    switch (flow) {
    case FLOW_OFF:
        this->flow = FLOW_OFF;
        break;
    case FLOW_HARDWARE:
        this->flow = FLOW_HARDWARE;
        break;
    case FLOW_XONXOFF:
        this->flow = FLOW_XONXOFF;
        break;
    default:
        // If none of the above cases matches, there must be an error
        accepted = false;
        break;
    }
    port->setFlowControl(this->flow);
    if(reconnect) open();
    return accepted;
}

bool SerialLink::setParityType(int parity) {
    bool reconnect = false;
    bool accepted = true;
    if(isConnected()) {
        disconnect();
        reconnect = true;
    }

    switch (parity) {
    case PAR_NONE:
        this->parity = PAR_NONE;
        break;
    case PAR_ODD:
        this->parity = PAR_ODD;
        break;
    case PAR_EVEN:
        this->parity = PAR_EVEN;
        break;
    case PAR_MARK:
        this->parity = PAR_MARK;
        break;
    case PAR_SPACE:
        this->parity = PAR_SPACE;
        break;
    default:
        // If none of the above cases matches, there must be an error
        accepted = false;
        break;
    }

    port->setParity(this->parity);
    if(reconnect) open();
    return accepted;
}

bool SerialLink::setDataBitsType(int dataBits) {
    bool accepted = true;

    switch (dataBits) {
    case 5:
        this->dataBits = DATA_5;
        break;
    case 6:
        this->dataBits = DATA_6;
        break;
    case 7:
        this->dataBits = DATA_7;
        break;
    case 8:
        this->dataBits = DATA_8;
        break;
    default:
        // If none of the above cases matches, there must be an error
        accepted = false;
        break;
    }

    port->setDataBits(this->dataBits);
    if(isConnected()) {
        close();
        open();
    }

    return accepted;
}

bool SerialLink::setStopBitsType(int stopBits) {
    bool reconnect = false;
    bool accepted = true;
    if(isConnected()) {
        close();
        reconnect = true;
    }

    switch (stopBits) {
    case 1:
        this->stopBits = STOP_1;
        break;
    case 2:
        this->stopBits = STOP_2;
        break;
    default:
        // If none of the above cases matches, there must be an error
        accepted = false;
        break;
    }

    port->setStopBits(this->stopBits);
    if(reconnect) open();
    return accepted;
}

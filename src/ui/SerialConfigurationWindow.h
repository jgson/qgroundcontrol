/*=====================================================================

PIXHAWK Micro Air Vehicle Flying Robotics Toolkit
Please see our website at <http://pixhawk.ethz.ch>

(c) 2009 PIXHAWK PROJECT  <http://pixhawk.ethz.ch>

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
 *   @brief Definition of configuration window for serial links
 *
 *   @author Lorenz Meier <mavteam@student.ethz.ch>
 *   @author Michael Schulz <coding@calihem.de>
 *
 */

#ifndef _SERIALCONFIGURATIONWINDOW_H_
#define _SERIALCONFIGURATIONWINDOW_H_

#include <QObject>
#include <QWidget>
#include <QAction>
#include <QTimer>
#include <QShowEvent>
#include <QHideEvent>
#include <LinkInterface.h>
#include <SerialLinkInterface.h>
#include "ui_SerialSettings.h"

class SerialConfigurationWindow : public QWidget
{
	Q_OBJECT

	public:
		SerialConfigurationWindow(SerialLinkInterface* link, QWidget *parent = 0, Qt::WindowFlags flags = Qt::Sheet);
		~SerialConfigurationWindow();

	public slots:
		void refreshPortNameComboBox();
		/// Set the window title to given @param portname
		void setTitleToPortName(const QString& portname);
		/// Enable flow control of serial link
		void enableFlowControl(bool flow);

	private slots:
		void setPortName(const QString& portname);
		void setBaudRate(int index);
		void setParityNone();
		void setParityOdd();
		void setParityEven();
		void setDataBits(int bits);
		void setStopBits(int bits);

	protected:

	private:
		Ui::serialSettings ui;
		SerialLinkInterface* serialLink;

		/// fills the port name combobox with ports found on system
		void fillPortNameComboBox();
		/// fills the baud rate combobox with baud rates supported by system
		void fillBaudRateComboBox();
		/// Setup the user interface according to link type
		void loadSettingsFromPort();
		void loadPortNameFromPort();
		void setupSignals();

};


#endif // _SERIALCONFIGURATIONWINDOW_H_

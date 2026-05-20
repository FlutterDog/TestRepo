<?xml version='1.0' encoding='UTF-8'?>
<Project Type="Project" LVVersion="23008000">
	<Item Name="My Computer" Type="My Computer">
		<Property Name="server.app.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.control.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.tcp.enabled" Type="Bool">false</Property>
		<Property Name="server.tcp.port" Type="Int">0</Property>
		<Property Name="server.tcp.serviceName" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.tcp.serviceName.default" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.vi.callsEnabled" Type="Bool">true</Property>
		<Property Name="server.vi.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="specify.custom.address" Type="Bool">false</Property>
		<Item Name="ICON_NewSt.png.ico" Type="Document" URL="../../../../../../../../IGAS/Distribu/Design/ICON_NewSt.png.ico"/>
		<Item Name="LAI Box.vi" Type="VI" URL="../LAI Box.vi"/>
		<Item Name="MB Modbus Command.ctl" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Modbus Command.ctl"/>
		<Item Name="WriteMultRegSplit.vi" Type="VI" URL="../SubVI/WriteMultRegSplit.vi"/>
		<Item Name="Dependencies" Type="Dependencies">
			<Item Name="vi.lib" Type="Folder">
				<Item Name="BuildHelpPath.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/BuildHelpPath.vi"/>
				<Item Name="Check Special Tags.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Check Special Tags.vi"/>
				<Item Name="Clear Errors.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Clear Errors.vi"/>
				<Item Name="Close File+.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Close File+.vi"/>
				<Item Name="compatReadText.vi" Type="VI" URL="/&lt;vilib&gt;/_oldvers/_oldvers.llb/compatReadText.vi"/>
				<Item Name="Convert property node font to graphics font.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Convert property node font to graphics font.vi"/>
				<Item Name="Details Display Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Details Display Dialog.vi"/>
				<Item Name="DialogType.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/DialogType.ctl"/>
				<Item Name="DialogTypeEnum.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/DialogTypeEnum.ctl"/>
				<Item Name="Error Code Database.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Error Code Database.vi"/>
				<Item Name="ErrWarn.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/ErrWarn.ctl"/>
				<Item Name="eventvkey.ctl" Type="VI" URL="/&lt;vilib&gt;/event_ctls.llb/eventvkey.ctl"/>
				<Item Name="ex_CorrectErrorChain.vi" Type="VI" URL="/&lt;vilib&gt;/express/express shared/ex_CorrectErrorChain.vi"/>
				<Item Name="Find First Error.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Find First Error.vi"/>
				<Item Name="Find Tag.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Find Tag.vi"/>
				<Item Name="Format Message String.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Format Message String.vi"/>
				<Item Name="General Error Handler Core CORE.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/General Error Handler Core CORE.vi"/>
				<Item Name="General Error Handler.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/General Error Handler.vi"/>
				<Item Name="Get String Text Bounds.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Get String Text Bounds.vi"/>
				<Item Name="Get Text Rect.vi" Type="VI" URL="/&lt;vilib&gt;/picture/picture.llb/Get Text Rect.vi"/>
				<Item Name="GetHelpDir.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/GetHelpDir.vi"/>
				<Item Name="GetRTHostConnectedProp.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/GetRTHostConnectedProp.vi"/>
				<Item Name="Longest Line Length in Pixels.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Longest Line Length in Pixels.vi"/>
				<Item Name="LVBoundsTypeDef.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/miscctls.llb/LVBoundsTypeDef.ctl"/>
				<Item Name="LVRectTypeDef.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/miscctls.llb/LVRectTypeDef.ctl"/>
				<Item Name="LVStringsAndValuesArrayTypeDef_U16.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/miscctls.llb/LVStringsAndValuesArrayTypeDef_U16.ctl"/>
				<Item Name="MB CRC-16.vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB CRC-16.vi"/>
				<Item Name="MB Decode Data.vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Decode Data.vi"/>
				<Item Name="MB LRC-8.vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB LRC-8.vi"/>
				<Item Name="MB Modbus Command to Data Unit.vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Modbus Command to Data Unit.vi"/>
				<Item Name="MB Modbus Data Unit.ctl" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Modbus Data Unit.ctl"/>
				<Item Name="MB Serial Master Query (poly).vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query (poly).vi"/>
				<Item Name="MB Serial Master Query Read Coils (poly).vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query Read Coils (poly).vi"/>
				<Item Name="MB Serial Master Query Read Discrete Inputs (poly).vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query Read Discrete Inputs (poly).vi"/>
				<Item Name="MB Serial Master Query Read Exception Status (poly).vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query Read Exception Status (poly).vi"/>
				<Item Name="MB Serial Master Query Read Holding Registers (poly).vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query Read Holding Registers (poly).vi"/>
				<Item Name="MB Serial Master Query Read Input Registers (poly).vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query Read Input Registers (poly).vi"/>
				<Item Name="MB Serial Master Query Write Multiple Coils (poly).vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query Write Multiple Coils (poly).vi"/>
				<Item Name="MB Serial Master Query Write Multiple Registers (poly).vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query Write Multiple Registers (poly).vi"/>
				<Item Name="MB Serial Master Query Write Single Coil (poly).vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query Write Single Coil (poly).vi"/>
				<Item Name="MB Serial Master Query Write Single Register (poly).vi" Type="VI" URL="/&lt;vilib&gt;/MB Serial Master Query Write Single Register (poly).vi"/>
				<Item Name="MB Serial Master Query Write Single Register (poly).vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query Write Single Register (poly).vi"/>
				<Item Name="MB Serial Master Query.vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Master Query.vi"/>
				<Item Name="MB Serial Modbus Data Unit to String.vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Modbus Data Unit to String.vi"/>
				<Item Name="MB Serial Receive.vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Receive.vi"/>
				<Item Name="MB Serial String to Modbus Data Unit.vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial String to Modbus Data Unit.vi"/>
				<Item Name="MB Serial Transmit.vi" Type="VI" URL="/&lt;vilib&gt;/NI Modbus.llb/MB Serial Transmit.vi"/>
				<Item Name="Not Found Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Not Found Dialog.vi"/>
				<Item Name="Open File+.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Open File+.vi"/>
				<Item Name="Read Delimited Spreadsheet (DBL).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Read Delimited Spreadsheet (DBL).vi"/>
				<Item Name="Read Delimited Spreadsheet (I64).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Read Delimited Spreadsheet (I64).vi"/>
				<Item Name="Read Delimited Spreadsheet (string).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Read Delimited Spreadsheet (string).vi"/>
				<Item Name="Read Delimited Spreadsheet.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Read Delimited Spreadsheet.vi"/>
				<Item Name="Read File+ (string).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Read File+ (string).vi"/>
				<Item Name="Read Lines From File (with error IO).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Read Lines From File (with error IO).vi"/>
				<Item Name="Search and Replace Pattern.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Search and Replace Pattern.vi"/>
				<Item Name="Set Bold Text.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Set Bold Text.vi"/>
				<Item Name="Set String Value.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Set String Value.vi"/>
				<Item Name="Simple Error Handler.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Simple Error Handler.vi"/>
				<Item Name="subFile Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/express/express input/FileDialogBlock.llb/subFile Dialog.vi"/>
				<Item Name="TagReturnType.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/TagReturnType.ctl"/>
				<Item Name="Three Button Dialog CORE.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Three Button Dialog CORE.vi"/>
				<Item Name="Three Button Dialog.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Three Button Dialog.vi"/>
				<Item Name="Trim Whitespace One-Sided.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Trim Whitespace One-Sided.vi"/>
				<Item Name="Trim Whitespace.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Trim Whitespace.vi"/>
				<Item Name="VISA Find Search Mode.ctl" Type="VI" URL="/&lt;vilib&gt;/Instr/_visa.llb/VISA Find Search Mode.ctl"/>
				<Item Name="VISA Open Access Mode.ctl" Type="VI" URL="/&lt;vilib&gt;/Instr/_visa.llb/VISA Open Access Mode.ctl"/>
				<Item Name="whitespace.ctl" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/whitespace.ctl"/>
				<Item Name="Write Delimited Spreadsheet (DBL).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Write Delimited Spreadsheet (DBL).vi"/>
				<Item Name="Write Delimited Spreadsheet (I64).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Write Delimited Spreadsheet (I64).vi"/>
				<Item Name="Write Delimited Spreadsheet (string).vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Write Delimited Spreadsheet (string).vi"/>
				<Item Name="Write Delimited Spreadsheet.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Write Delimited Spreadsheet.vi"/>
				<Item Name="Write Spreadsheet String.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Write Spreadsheet String.vi"/>
			</Item>
			<Item Name="2WordTOfloat.vi" Type="VI" URL="/C/Development/MONITORING/LCT_STOCK/LCT_Toolbox/SubVI/2WordTOfloat.vi"/>
			<Item Name="BlankArray.vi" Type="VI" URL="../SubVI/BlankArray.vi"/>
			<Item Name="CommIndicator.vi" Type="VI" URL="../SubVI/CommIndicator.vi"/>
			<Item Name="Delay.vi" Type="VI" URL="../SubVI/Delay.vi"/>
			<Item Name="DialogBox.vi" Type="VI" URL="../SubVI/DialogBox.vi"/>
			<Item Name="GasData.vi" Type="VI" URL="../SubVI/GasData.vi"/>
			<Item Name="Get Connection to Detector.vi" Type="VI" URL="../SubVI/Get Connection to Detector.vi"/>
			<Item Name="GetDeviceName.vi" Type="VI" URL="../SubVI/GetDeviceName.vi"/>
			<Item Name="GetSerialNum.vi" Type="VI" URL="../SubVI/GetSerialNum.vi"/>
			<Item Name="Init Port.vi" Type="VI" URL="../SubVI/Init Port.vi"/>
			<Item Name="LoadGasCalibr.vi" Type="VI" URL="../SubVI/LoadGasCalibr.vi"/>
			<Item Name="ModeSelect.vi" Type="VI" URL="../SubVI/ModeSelect.vi"/>
			<Item Name="Read Register.vi" Type="VI" URL="../SubVI/Read Register.vi"/>
			<Item Name="ReadHregMulty.vi" Type="VI" URL="../SubVI/ReadHregMulty.vi"/>
			<Item Name="SaveTempCalibrLog.vi" Type="VI" URL="../SubVI/SaveTempCalibrLog.vi"/>
			<Item Name="SubVi.vi" Type="VI" URL="../SubVI/SubVi.vi"/>
			<Item Name="TaskScheduller.vi" Type="VI" URL="../SubVI/TaskScheduller.vi"/>
			<Item Name="WriteMultReg.vi" Type="VI" URL="../SubVI/WriteMultReg.vi"/>
			<Item Name="WriteReg.vi" Type="VI" URL="../SubVI/WriteReg.vi"/>
		</Item>
		<Item Name="Build Specifications" Type="Build">
			<Item Name="LAI Box" Type="EXE">
				<Property Name="App_copyErrors" Type="Bool">true</Property>
				<Property Name="App_INI_aliasGUID" Type="Str">{A4F6D100-6B66-4570-82AE-1077C8B7B79B}</Property>
				<Property Name="App_INI_GUID" Type="Str">{8A38E250-D695-4644-9A76-CB5A6037F91B}</Property>
				<Property Name="App_serverConfig.httpPort" Type="Int">8002</Property>
				<Property Name="App_serverType" Type="Int">1</Property>
				<Property Name="Bld_buildCacheID" Type="Str">{0B935BB1-EFB6-4032-BE59-D27B249FCF10}</Property>
				<Property Name="Bld_buildSpecName" Type="Str">LAI Box</Property>
				<Property Name="Bld_excludeInlineSubVIs" Type="Bool">true</Property>
				<Property Name="Bld_excludeLibraryItems" Type="Bool">true</Property>
				<Property Name="Bld_excludePolymorphicVIs" Type="Bool">true</Property>
				<Property Name="Bld_localDestDir" Type="Path">/C/Development/MONITORING/LAI/EXE</Property>
				<Property Name="Bld_modifyLibraryFile" Type="Bool">true</Property>
				<Property Name="Bld_previewCacheID" Type="Str">{6FEBF4F2-C921-4E12-8183-4DD91D9EEC5F}</Property>
				<Property Name="Bld_version.build" Type="Int">8</Property>
				<Property Name="Bld_version.major" Type="Int">5</Property>
				<Property Name="Bld_version.minor" Type="Int">5</Property>
				<Property Name="Destination[0].destName" Type="Str">LAIbox.exe</Property>
				<Property Name="Destination[0].path" Type="Path">/C/Development/MONITORING/LAI/EXE/LAI Box.exe</Property>
				<Property Name="Destination[0].path.type" Type="Str">&lt;none&gt;</Property>
				<Property Name="Destination[0].preserveHierarchy" Type="Bool">true</Property>
				<Property Name="Destination[0].type" Type="Str">App</Property>
				<Property Name="Destination[1].destName" Type="Str">Support Directory</Property>
				<Property Name="Destination[1].path" Type="Path">/C/Development/MONITORING/LAI/EXE/data</Property>
				<Property Name="Destination[1].path.type" Type="Str">&lt;none&gt;</Property>
				<Property Name="DestinationCount" Type="Int">2</Property>
				<Property Name="Exe_iconItemID" Type="Ref">/My Computer/ICON_NewSt.png.ico</Property>
				<Property Name="Source[0].itemID" Type="Str">{1C5C32BC-3C28-4C9C-A542-1595D93C43BF}</Property>
				<Property Name="Source[0].type" Type="Str">Container</Property>
				<Property Name="Source[1].destinationIndex" Type="Int">0</Property>
				<Property Name="Source[1].itemID" Type="Ref">/My Computer/LAI Box.vi</Property>
				<Property Name="Source[1].sourceInclusion" Type="Str">TopLevel</Property>
				<Property Name="Source[1].type" Type="Str">VI</Property>
				<Property Name="SourceCount" Type="Int">2</Property>
				<Property Name="TgtF_fileDescription" Type="Str">LAI Box</Property>
				<Property Name="TgtF_internalName" Type="Str">LAI Box</Property>
				<Property Name="TgtF_legalCopyright" Type="Str">Copyright © 2019 </Property>
				<Property Name="TgtF_productName" Type="Str">LAI Box</Property>
				<Property Name="TgtF_targetfileGUID" Type="Str">{A4B57644-9036-4C18-8DDB-9DD76E1F93BE}</Property>
				<Property Name="TgtF_targetfileName" Type="Str">LAIbox.exe</Property>
			</Item>
			<Item Name="Toolbox Installer" Type="Installer">
				<Property Name="Destination[0].name" Type="Str">c:\IGAS</Property>
				<Property Name="Destination[0].path" Type="Path">/c/IGAS</Property>
				<Property Name="Destination[0].tag" Type="Str">{6C63F5C5-F5FA-45A6-B846-6E95731B657C}</Property>
				<Property Name="Destination[0].type" Type="Str">absFolder</Property>
				<Property Name="Destination[1].name" Type="Str">Toolbox 5.5</Property>
				<Property Name="Destination[1].parent" Type="Str">{6C63F5C5-F5FA-45A6-B846-6E95731B657C}</Property>
				<Property Name="Destination[1].tag" Type="Str">{17290EBC-68C2-4BE7-86FE-B564F11ED09E}</Property>
				<Property Name="Destination[1].type" Type="Str">userFolder</Property>
				<Property Name="DestinationCount" Type="Int">2</Property>
				<Property Name="INST_autoIncrement" Type="Bool">true</Property>
				<Property Name="INST_buildLocation" Type="Path">../builds/ToolboxInstaller/Toolbox Installer</Property>
				<Property Name="INST_buildLocation.type" Type="Str">relativeToCommon</Property>
				<Property Name="INST_buildSpecName" Type="Str">Toolbox Installer</Property>
				<Property Name="INST_defaultDir" Type="Str">{3912416A-D2E5-411B-AFEE-B63654D690C0}</Property>
				<Property Name="INST_productName" Type="Str">ToolboxInstaller</Property>
				<Property Name="INST_productVersion" Type="Str">1.0.7</Property>
				<Property Name="InstSpecBitness" Type="Str">32-bit</Property>
				<Property Name="InstSpecVersion" Type="Str">23108276</Property>
				<Property Name="MSI_arpCompany" Type="Str">IGAS Ltd</Property>
				<Property Name="MSI_arpURL" Type="Str">igasdetection.com</Property>
				<Property Name="MSI_distID" Type="Str">{454BFDA0-6D6B-474E-9FED-91438F7E45A6}</Property>
				<Property Name="MSI_osCheck" Type="Int">0</Property>
				<Property Name="MSI_upgradeCode" Type="Str">{29740A76-3D31-4DCA-B471-9B75A487125C}</Property>
				<Property Name="RegDest[0].dirName" Type="Str">Software</Property>
				<Property Name="RegDest[0].dirTag" Type="Str">{DDFAFC8B-E728-4AC8-96DE-B920EBB97A86}</Property>
				<Property Name="RegDest[0].parentTag" Type="Str">2</Property>
				<Property Name="RegDestCount" Type="Int">1</Property>
				<Property Name="Source[0].dest" Type="Str">{17290EBC-68C2-4BE7-86FE-B564F11ED09E}</Property>
				<Property Name="Source[0].File[0].dest" Type="Str">{17290EBC-68C2-4BE7-86FE-B564F11ED09E}</Property>
				<Property Name="Source[0].File[0].name" Type="Str">Toolbox.exe</Property>
				<Property Name="Source[0].File[0].Shortcut[0].destIndex" Type="Int">0</Property>
				<Property Name="Source[0].File[0].Shortcut[0].name" Type="Str">Toolbox</Property>
				<Property Name="Source[0].File[0].Shortcut[0].subDir" Type="Str">AluFlon Calibration</Property>
				<Property Name="Source[0].File[0].ShortcutCount" Type="Int">1</Property>
				<Property Name="Source[0].File[0].tag" Type="Str">{A4B57644-9036-4C18-8DDB-9DD76E1F93BE}</Property>
				<Property Name="Source[0].FileCount" Type="Int">1</Property>
				<Property Name="Source[0].name" Type="Str">Toolbox 5.5</Property>
				<Property Name="Source[0].tag" Type="Ref">/My Computer/Build Specifications/LAI Box</Property>
				<Property Name="Source[0].type" Type="Str">EXE</Property>
				<Property Name="SourceCount" Type="Int">1</Property>
			</Item>
		</Item>
	</Item>
</Project>

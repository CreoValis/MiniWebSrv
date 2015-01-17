
var TestWSConn=null;

function AppendWSLog(Target, EntryClass, Message)
{
	if (Target.childNodes.length>=50)
		Target.removeChild(Target.firstChild);

	var NewEntry=document.createElement("span");
	NewEntry.setAttribute("class",EntryClass);
	NewEntry.appendChild(document.createTextNode(Message));
	Target.appendChild(NewEntry);
}

function ConnectWS(StatusTargetId,TargetId)
{	
	function SetStatusDisp(Target, Status)
	{
		while (Target.childNodes.length)
			Target.removeChild(Target.firstChild);

		Target.appendChild(document.createTextNode(Status));
	}
	
	if (TestWSConn)
		return;

	TestWSConn=new WebSocket("ws://" + location.host + "/echo","echo");
	TestWSConn.LogTarget=document.getElementById(TargetId);
	TestWSConn.StatusTarget=document.getElementById(StatusTargetId);
	TestWSConn.onopen=function(Evt) { AppendWSLog(TestWSConn.LogTarget,"sys","Connection opened."); SetStatusDisp(TestWSConn.StatusTarget,"Open"); }
	TestWSConn.onclose=function(Evt)
	{
		AppendWSLog(TestWSConn.LogTarget,"sys","Connection closed.");
		SetStatusDisp(TestWSConn.StatusTarget,"Closed");
		if (TestWSConn)
			window.setTimeout(ConnectWS,2000);
	};
	TestWSConn.onerror=function(Evt) { AppendWSLog(TestWSConn.LogTarget,"sys","Got error event(" + Evt.type + ")."); };
	TestWSConn.onmessage=function(Evt)
	{
		if (!TestWSConn)
		{
			try { TestWSConn.close(); }
			catch (Err) { }
			return;
		}

		AppendWSLog(TestWSConn.LogTarget,"recv","Recv: " + Evt.data);
	};
}

function DisconnectWS()
{
	if (TestWSConn)
	{
		var PrevWS=TestWSConn;
		TestWSConn=null;

		try { PrevWS.close(); }
		catch (Err) { }
	}
}

function SendWS(SourceId)
{
	if (!TestWSConn)
		return;

	var Msg=document.getElementById(SourceId).value;
	AppendWSLog(TestWSConn.LogTarget,"send","Send: " + Msg);
	TestWSConn.send(Msg);
}

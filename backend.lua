function pulse()
    dbg("Pulse")
    if not C then dbg("No C") return end
    
end
function S2W(x,y,width, height)
	if not C then dbg("No C S2W()") return end
	hitx,hity,hitz = C("S2W",x,y,width,height)
    return hitx,hity,hitz
end)
function Trace(px,py,pz,tx,ty,tz,flags)
    bool,x,y,z = C("Trace",px,py,pz,tx,ty,tz,flags)
    return bool,x,y,z
end
function W2S(locx,locy,locz)
    screenPosX,screenPosY,width = C("W2S",locx,locy,locz)
    return screenPosX,screenPosY,width
end
function Facing(objPointer)
	if not C then dbg("No C Facing()") return end
	return C("Facing",objpointer)
end
function Field(objPointer,offset,guidBool,smallIntBool)
    if not C then dbg("No C Field()") return 
    return C("Field",objPointer,offset,guidBool,smallIntBool))
end
function GuidFromPointer(objPointer)
	if not C then dbg("No C GuidFromPointer()") return end
	return C("GUID",objPointer)
end
function Interact(objPointer)
    if not C then dbg("No C Interact()") return end
    C("Interact",objPointer)
end
function Position(objPointer)
	if not C then dbg("No C Position()") return end
	return C("Position",objPointer)
end
function TypeInt(objPointer)
    if not C then dbg("No C TypeInt()") return end
    return C("Type",objPointer)
end
function AppPath()
	if not C then dbg("No C AppPath()") return end
	return C("AppPath")
end
function FileExists(filePath)
	if not C then dbg("No C FileExists()") return end
	return C("FileExists",filePath)
end
function FileType(filePath)
	if not C then dbg("No C FileType()") return end
	return C("FileType",path)
end
function LoadFile(filePath)
	if not C then dbg("No C LoadFile()") return end
	return C("LoadFile",filePath)
end)
function NewFolder(folderPath)
	if not C then dbg("No C NewFolder()") return end
	return C("NewFolder",folderPath)
end
function SourcePath()
	if not C then dbg("No C SourcePath()") return end
	return C("SourcePath")
end
function WriteFile(filePath,context,appendBool)
	if not C then dbg("No C Write()") return end
	return C("WriteFile",filePath,context,appendBool)
end
function AppBase()
	if not C then dbg("No C AppBase()") return end
	return C("AppBase")
end
function ObjSearch(guid_or_name)
	if not C then dbg("No C ObjSearch()") return end
	return C("ObjSearch",guid_or_name)
end
function Afk()
	if not C then dbg("No C Afk()") return end
	return C("Afk")
end
function MouseClick(x,y,z,left_or_right_bool)
	if not C then dbg("No C MouseClick()") return end
	return C("Click",x,y,z,left_or_right_bool)
end



function out(context)
    if not C then dbg("No C out()") return end
    dbg("Out: "..context)
    C("Write","C:\\out\\out.lua",context.."\n",true)
end
function pos(object)
    if not C then dbg("No C pos()") return end
    local x,y,z = C("Position",object)
    dbg("pos("..object..") = "..x..","..y..","..z)
    return x,y,z
end
function posString(object)
    if not C then dbg("No C posString()") return end
    local x,y,z = C("Position",object)
    return x..","..y..","..z
end
function dist(...)
    if not C then dbg("No C dist()") return end
    local tar = 'target'
    x1,y1,z1,x2,y2,z2 = ...
    if z2 then
        local x,y,z = x1,y1,z1
        local otherx,othery,otherz = x2,y2,z2
        local distance = math.sqrt(((x-otherx) * (x-otherx)) + ((y-othery) * (y-othery)) + ((z-otherz) * (z-otherz)))
        return distance
    elseif x2 then
        local x,y,z
        local otherx,othery,otherz
        if type(x1) == "table" then
            if x1.x then
                x,y,z = x1.x,x1.y,x1.z
            else
                x,y,z = x1[1],x1[2],x1[3]
            end
        else
            x,y,z = C("Position",UnitGUID('player'))
        end
        if type(x2) == "table" then
            if x2.x then
                otherx,othery,otherz = x2.x,x2.y,x2.z
            else
                otherx,othery,otherz = x2[1],x2[2],x2[3]
            end
        else
            otherx,othery,otherz = y1,z1,x2
        end
        
        local distance = math.sqrt(((x-otherx) * (x-otherx)) + ((y-othery) * (y-othery)) + ((z-otherz) * (z-otherz)))
        return distance
    elseif z1  then
        local x,y,z = C("MyPosition")
        local otherx,othery,otherz = x1,y1,z1
        local distance = math.sqrt(((x-otherx) * (x-otherx)) + ((y-othery) * (y-othery)) + ((z-otherz) * (z-otherz)))
        return distance
    elseif y1 then
        local x,y,z
        local otherx,othery,otherz
        if C("Postion",x1) then
            x,y,z = C("Position",x1)
        else
            x,y,z = x1[1],x1[2],x1[3]
        end
        if type(y1) == "table" then
            otherx,othery,otherz = y1[1],y1[2],y1[3]
        else
            otherx,othery,otherz = C("Position",y1)
        end
        local distance = math.sqrt(((x-otherx) * (x-otherx)) + ((y-othery) * (y-othery)) + ((z-otherz) * (z-otherz)))
        return distance
    elseif x1 then
        local x,y,z = C("Position",UnitGUID('player'))
        local otherx,othery,otherz
        if type(x1) == "table" then
            if x1.x then
                otherx,othery,otherz = x1.x,x1.y,x1.z
            else
                otherx,othery,otherz = x1[1],x1[2],x1[3]
            end
        else
            otherx,othery,otherz = C("Position",x1)
        end
        local distance = math.sqrt(((x-otherx) * (x-otherx)) + ((y-othery) * (y-othery)) + ((z-otherz) * (z-otherz)))
        return distance
    end
end
function afk()
    if not C then dbg("No C afk()") return end
    C("AFK")
end
function clickGUID(guid)
    if not C then dbg("No C clickGUID()") return end
    C("ClickGUID",guid)
end
function clickName(name)
    if not C then dbg("No C clickName()") return end
    C("ClickName",name)
end
function CTM(x,y,z)
    if not C then dbg("No C CTM()") return end
    C("CTM",x,y,z)
end
function getFacing(guid)
    if not C then dbg("No C getFacing()") return end
    return C("Facing",guid)
end
function indexToGUID(index)
    if not C then dbg("No C indexToGUID()") return end
    return C("IndexToGUID",index)
end
function loadFile(path)
    if not C then dbg("No C loadFile()") return end
    return C("Load",path)
end
function myPos()
    if not C then dbg("No C myPos()") return end
    return C("MyPosition")
end
function objectCount()
    if not C then dbg("No C objectCount()") return end
    return C("ObjectCount")
end
function objName(guid)
    if not C then dbg("No C objName()") return end
    return C("Name",guid)
end
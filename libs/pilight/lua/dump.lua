function dump_(o, i)
	if type(o) == 'table' then
		local s = '{\n'
		for k,v in pairs(o) do
			if type(k) ~= 'number' then k = '"'..k..'"' end
				s = s .. string.rep("\t", i) .. '['..k..'] = ' .. dump_(v, i+1) .. ',\n'
			end
		return s .. string.rep("\t", i-1) .. '}'
	else
		return tostring(o)
	end
end

function dump(o)
	return dump_(o, 1);
end

return dump;
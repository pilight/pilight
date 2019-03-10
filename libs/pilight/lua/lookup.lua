function lookup(t, ...)
	for _, k in ipairs({...}) do
		t = t[k]
		if not t then
			return nil
		end
	end
	return t
end

return lookup;
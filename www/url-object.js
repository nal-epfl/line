// See: http://www.thecodeship.com/web-development/javascript-url-object/
function urlObject(options)
{
   default_options = {'url':window.location.href,'unescape':true,'convert_num':true};

   if(typeof options !== "object")
      options = default_options;
   else
   {
      for(var index in default_options)
      {
         if(typeof options[index] ==="undefined")
            options[index] = default_options[index];
      }
   }
 
   var a = document.createElement('a');
   a.href = options['url'];
   url_query = a.search.substring(1);

   var params = {};
   var vars = url_query.split('&');

   if(vars[0].length > 1)
   {
      for(var i = 0; i < vars.length; i++)
      {
        var pair = vars[i].split("=");
        var key = (options['unescape'])?unescape(pair[0]):pair[0];
        var val = (options['unescape'])?unescape(pair[1]):pair[1];
 
        if(options['convert_num'])
        {	
           if(val.match(/^\d+$/))
             val = parseInt(val);
 
           else if(val.match(/^\d+\.\d+$/))
             val = parseFloat(val);
        }

        if(typeof params[key] === "undefined")
           params[key] = val;
        else if(typeof params[key] === "string")
           params[key] = [params[key],val];
        else
           params[key].push(val);
      }	
   }

   var urlObj = {
      protocol:a.protocol,
      hostname:a.hostname,
      host:a.host,
      port:a.port,
      hash:a.hash,
      pathname:a.pathname,
      search:a.search,
      parameters:params
   };
 
   return urlObj;
}

param([string]$FilePath, [string]$AUTH_TOKEN)

    #$AUTH_TOKEN = 'QE0kiolxK190xripPcVNCMw'
    $Headers = @{'authorization' = "Bearer $AUTH_TOKEN" };
    $devices = Invoke-RestMethod -Uri 'https://mdash.net/api/v2/devices' -ContentType 'application/json' -Method Get -Headers $Headers 

    foreach ($item in $devices) {
    
        $URL_ota = "http://dash.mongoose-os.com/api/v2/devices/$($item.id)/ota";
        #$URL_update = "https://mdash.net/api/v2/devices/$($item.id)/update";

        $fileBytes = [System.IO.File]::ReadAllBytes($FilePath);
        $fileEnc = [System.Text.Encoding]::GetEncoding('UTF-8').GetString($fileBytes);
        $boundary = [System.Guid]::NewGuid().ToString(); 
        $LF = "`r`n";

        $bodyLines = ( 
            "--$boundary",
            "Content-Disposition: form-data; name=`"file`"; filename=`"fw.zip`"",
            "Content-Type: application/octet-stream$LF",
            $fileEnc,
            "--$boundary--$LF" 
        ) -join $LF

        Invoke-RestMethod -Uri $URL_ota -Method Post -ContentType "multipart/form-data; boundary=`"$boundary`"" -Body $bodyLines -Headers $Headers
        #Invoke-RestMethod -Uri $URL_update -Method Post -ContentType "multipart/form-data; boundary=`"$boundary`"" -Body $bodyLines -Headers $Headers
    } 
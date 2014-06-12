--
-- Copyright (c) 2012 Citrix Systems, Inc.
-- 
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
-- 
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
-- 
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
--

{-# LANGUAGE ScopedTypeVariables #-}
module Curl
       ( httpGet
       , httpPost
       , withCurlDo
       , URLString
       , CurlError (..)
       )
       where

import qualified Data.ByteString.UTF8 as UTF8
import Network.Curl
import System.IO

debugOn = True
debug | debugOn   = hPutStrLn stderr
      | otherwise = const (return ())

curlConnectTimeout = 20
curlTimeout = 20
curlDNSCacheTimeout = 60

data CurlError = CurlError Int CurlCode

curlOpts
 =
       [ CurlConnectTimeout curlConnectTimeout
       , CurlTimeout curlTimeout
       , CurlDNSCacheTimeout curlDNSCacheTimeout
       , CurlFollowLocation True
-- FIXME (possibly)
       , CurlSSLVerifyPeer False
--       , CurlCAInfo "/home/tomaszw/CITRIXRootCA"
       , CurlHeader False
       , CurlNoProgress True
       ]

httpGet :: URLString -> IO (Either CurlError String)
httpGet url = do
  debug $ "send GET " ++ url
  r <- curlGetResponse_ url curlOpts
  let (_ :: [(String,String)]) = respHeaders r
  case respCurlCode r of
    CurlOK -> let x = dropBOM $ UTF8.toString (respBody r) in debug ("recv:\n" ++ x) >> return (Right x)
    ccode  -> return $ Left (CurlError (respStatus r) ccode)

httpPost :: URLString -> String -> IO (Either CurlError String)
httpPost url req = do
  debug $ "send: POST " ++ url ++ "\n" ++ req
  r <- curlGetResponse_ url $ curlOpts ++
       [
           CurlPost True
         , CurlHttpHeaders [ "Content-Type: text/xml" ]
         , CurlPostFields [ req ]
       ]
  let (_ :: [(String,String)]) = respHeaders r
  case respCurlCode r of
    CurlOK -> let x = dropBOM $ UTF8.toString (respBody r) in debug ("recv:\n" ++ x) >> return (Right x)
    ccode  -> return $ Left (CurlError (respStatus r) ccode)

dropBOM :: String -> String
dropBOM [] = []
dropBOM s@(x:xs) =
  let unicodeMarker = '\65279' -- UTF-8 BOM
  in if x == unicodeMarker then xs else s

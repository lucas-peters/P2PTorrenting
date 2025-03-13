import axios from "axios";
import React, { useEffect } from "react";
import { BASE_URL } from "./App";

export const ClientNode = ({ container, setRetryGetContainer }) => {
  const [loading, setLoading] = React.useState(false);
  const [files, setFiles] = React.useState([]);
  const [torrents, setTorrents] = React.useState([]);
  const [activeTorrents, setActiveTorrents] = React.useState([]);

  const [generatingTorrent, setGeneratingTorrent] = React.useState({});

  const [dhtStats, setDhtStats] = React.useState({});

  const [retry, setRetry] = React.useState(0);

  useEffect(() => {
    const interval = setInterval(() => {
      setRetry((prevCount) => prevCount + 1);
    }, 1000);

    return () => clearInterval(interval); // Cleanup function to clear the interval
  }, []);

  const CLIENT_API_PORT_ON_HOST_MACHINE = container.Ports.filter(
    (port) => port.PrivatePort === 8080
  )[0]?.PublicPort;

  const CLIENT_BASE_API_URL = `http://localhost:${CLIENT_API_PORT_ON_HOST_MACHINE}`;

  const removeNode = async () => {
    setLoading(true);
    await axios.post(BASE_URL + `/containers/${container["Id"]}/kill`);
    await axios
      .delete(BASE_URL + `/containers/${container["Id"]}`)
      .then(() => {
        setRetryGetContainer((prev) => prev + 1);
      })
      .catch((err) => console.log(err));
  };

  const generateTorrent = async (path) => {
    setGeneratingTorrent({ ...generatingTorrent, [path]: true });
    try {
      await axios.post(`${CLIENT_BASE_API_URL}/generate-torrent`, {
        path,
      });
    } catch (err) {
      console.error(err);
    }
    setGeneratingTorrent({ ...generatingTorrent, [path]: false });
  };

  const addTorrent = async (file) => {
    try {
      await axios.post(`${CLIENT_BASE_API_URL}/add-torrent`, {
        file,
      });
    } catch (err) {
      console.error(err);
    }
    setGeneratingTorrent({ ...generatingTorrent, [file]: false });
  };

  useEffect(() => {
    (async () => {
      try {
        const res = await axios.get(`${CLIENT_BASE_API_URL}/dht-stats`);
        setDhtStats(res.data);
      } catch (err) {}
    })();
  }, [CLIENT_BASE_API_URL]);

  useEffect(() => {
    (async () => {
      try {
        const res = await axios.get(`${CLIENT_BASE_API_URL}/status`);
        setActiveTorrents(JSON.stringify(res.data));
      } catch (err) {}
    })();
  }, [CLIENT_BASE_API_URL, retry]);

  useEffect(() => {
    const fetchFiles = async ({ path }) => {
      try {
        // Step 1: Create an exec instance
        const execResponse = await axios.post(
          `${BASE_URL}/containers/${container["Id"]}/exec`,
          {
            AttachStdout: true,
            AttachStderr: true,
            Tty: false,
            Cmd: ["ls", path],
          }
        );

        const execId = execResponse.data.Id;

        // Step 2: Start the exec instance
        const startResponse = await axios.post(
          `${BASE_URL}/exec/${execId}/start`,
          {
            Detach: false,
            Tty: false,
          },
          { responseType: "json" }
        );

        // Convert output to list
        return startResponse.data
          .trim()
          .split("\n")
          .map((file) => file.replace(/[^\x20-\x7E]/g, ""));
      } catch (error) {
        console.error("Error listing files:", error);
        return [];
      }
    };

    fetchFiles({ path: "/app/downloads" }).then((files) => setFiles(files));
    fetchFiles({ path: "/app/torrents" }).then((torrents) =>
      setTorrents(torrents)
    );
  }, [container]);

  return (
    <div style={{ border: "1px solid black", padding: 4, margin: 4 }}>
      <h3>{container["Names"][0]}</h3>
      <p>{JSON.stringify(dhtStats)}</p>
      <p>
        {container["NetworkSettings"]["Networks"]["p2p-network"]["IPAddress"]}
      </p>

      <div style={{ border: "1px solid black", padding: 4, margin: 4 }}>
        <h4>Files</h4>
        {files.map((file) => (
          <div style={{ display: "flex", flexDirection: "row", gap: 8 }}>
            {file}
            <button
              disabled={generatingTorrent[file]}
              onClick={() => {
                generateTorrent(file);
              }}
            >
              {generatingTorrent[file] ? "Generating..." : "Generate Torrent"}
            </button>
          </div>
        ))}
      </div>

      <div style={{ border: "1px solid black", padding: 4, margin: 4 }}>
        <h4>Torrent Files</h4>
        {torrents.map((torrent) => (
          <div style={{ display: "flex", flexDirection: "row", gap: 8 }}>
            {torrent}
            <button onClick={() => addTorrent(torrent)}>Add Torrent</button>
          </div>
        ))}
      </div>

      <div style={{ border: "1px solid black", padding: 4, margin: 4 }}>
        <h4>Active Torrents</h4>
        {activeTorrents}
      </div>

      {loading ? (
        <p>Removing...</p>
      ) : (
        <button onClick={removeNode}>REMOVE NODE</button>
      )}
    </div>
  );
};

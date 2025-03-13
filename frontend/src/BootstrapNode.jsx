import React from "react";
import { BASE_URL } from "./App";
import axios from "axios";

export const BootstrapNode = ({ container, setRetryGetContainer }) => {
  const [loading, setLoading] = React.useState(false);

  const removeNode = async () => {
    setLoading(true);
    await axios.post(BASE_URL + `/containers/${container["Id"]}/stop`);
    await axios
      .delete(BASE_URL + `/containers/${container["Id"]}`)
      .then(() => {
        setRetryGetContainer((prev) => prev + 1);
      })
      .catch((err) => console.log(err));
  };

  return (
    <div style={{ border: "1px solid black", padding: 4, margin: 4 }}>
      <h3>{container["Names"][0]}</h3>
      <p>
        {container["NetworkSettings"]["Networks"]["p2p-network"]["IPAddress"]}
      </p>
      {loading ? (
        <p>Removing...</p>
      ) : (
        <button onClick={removeNode}>REMOVE NODE</button>
      )}
    </div>
  );
};
